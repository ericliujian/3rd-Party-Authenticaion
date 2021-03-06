/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the terms found in the LICENSE file in the root of this source tree.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file sctp_primitives_server.c
    \brief Main server primitives
    \author Sebastien ROUX, Lionel GAUTHIER
    \date 2013
    \version 1.0
    @ingroup _sctp
*/

#include "sctp_primitives_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "assertions.h"
#include "dynamic_memory_check.h"
#include "sctp_common.h"

#define SCTP_RC_ERROR -1
#define SCTP_RC_NORMAL_READ 0
#define SCTP_RC_DISCONNECT 1
#define SCTP_RECV_BUFFER_SIZE (8192)

typedef struct sctp_association_s {
    struct sctp_association_s *next_assoc;  ///< Next association in the list
    struct sctp_association_s
        *previous_assoc;  ///< Previous association in the list
    int sd;               ///< Socket descriptor
    uint32_t ppid;        ///< Payload protocol Identifier
    uint16_t
        instreams;  ///< Number of input streams negociated for this connection
    uint16_t
        outstreams;  ///< Number of output strams negotiated for this connection
    sctp_assoc_id_t assoc_id;  ///< SCTP association id for the connection
    uint32_t messages_recv;  ///< Number of messages received on this connection
    uint32_t messages_sent;  ///< Number of messages sent on this connection

    struct sockaddr *peer_addresses;  ///< A list of peer addresses
    int nb_peer_addresses;
} sctp_association_t;

typedef struct sctp_descriptor_s {
    // List of connected peers
    struct sctp_association_s *available_connections_head;
    struct sctp_association_s *available_connections_tail;

    uint32_t number_of_connections;
    uint16_t nb_instreams;
    uint16_t nb_outstreams;
} sctp_descriptor_t;

typedef struct sctp_arg_s {
    int sd;
    uint32_t ppid;
} sctp_arg_t;

static struct sctp_descriptor_s sctp_desc;

// Thread used to handle sctp messages
static pthread_t assoc_thread;

static server_sctp_recv_callback handle_received_sctp_message;

// LOCAL FUNCTIONS prototypes
void *sctp_receiver_thread(void *args_p);

// Association list related local functions prototypes
static struct sctp_association_s *sctp_is_assoc_in_list(
    sctp_assoc_id_t assoc_id);
static struct sctp_association_s *sctp_add_new_peer(void);
static int sctp_handle_com_down(sctp_assoc_id_t assoc_id);
static void sctp_dump_list(void);

//------------------------------------------------------------------------------
void set_sctp_message_handler(server_sctp_recv_callback handler) {
    handle_received_sctp_message = handler;
}

//------------------------------------------------------------------------------
static struct sctp_association_s *sctp_add_new_peer(void) {
    struct sctp_association_s *new_sctp_descriptor =
        calloc(1, sizeof(struct sctp_association_s));

    if (new_sctp_descriptor == NULL) {
        printf("Failed to allocate memory for new peer (%s:%d)\n", __FILE__,
               __LINE__);
        return NULL;
    }

    new_sctp_descriptor->next_assoc = NULL;
    new_sctp_descriptor->previous_assoc = NULL;

    if (sctp_desc.available_connections_tail == NULL) {
        sctp_desc.available_connections_head = new_sctp_descriptor;
        sctp_desc.available_connections_tail =
            sctp_desc.available_connections_head;
    } else {
        new_sctp_descriptor->previous_assoc =
            sctp_desc.available_connections_tail;
        sctp_desc.available_connections_tail->next_assoc = new_sctp_descriptor;
        sctp_desc.available_connections_tail = new_sctp_descriptor;
    }

    sctp_desc.number_of_connections++;
    sctp_dump_list();
    return new_sctp_descriptor;
}

//------------------------------------------------------------------------------
static struct sctp_association_s *sctp_is_assoc_in_list(
    sctp_assoc_id_t assoc_id) {
    struct sctp_association_s *assoc_desc = NULL;

    if (assoc_id < 0) {
        return NULL;
    }

    for (assoc_desc = sctp_desc.available_connections_head; assoc_desc;
         assoc_desc = assoc_desc->next_assoc) {
        if (assoc_desc->assoc_id == assoc_id) {
            break;
        }
    }

    return assoc_desc;
}

//------------------------------------------------------------------------------
static int sctp_remove_assoc_from_list(sctp_assoc_id_t assoc_id) {
    struct sctp_association_s *assoc_desc = NULL;

    /*
     * Association not in the list
     */
    if ((assoc_desc = sctp_is_assoc_in_list(assoc_id)) == NULL) {
        return -1;
    }

    if (assoc_desc->next_assoc == NULL) {
        if (assoc_desc->previous_assoc == NULL) {
            /*
             * Head and tail
             */
            sctp_desc.available_connections_head =
                sctp_desc.available_connections_tail = NULL;
        } else {
            /*
             * Not head but tail
             */
            sctp_desc.available_connections_tail = assoc_desc->previous_assoc;
            assoc_desc->previous_assoc->next_assoc = NULL;
        }
    } else {
        if (assoc_desc->previous_assoc == NULL) {
            /*
             * Head but not tail
             */
            sctp_desc.available_connections_head = assoc_desc->next_assoc;
            assoc_desc->next_assoc->previous_assoc = NULL;
        } else {
            /*
             * Not head and not tail
             */
            assoc_desc->previous_assoc->next_assoc = assoc_desc->next_assoc;
            assoc_desc->next_assoc->previous_assoc = assoc_desc->previous_assoc;
        }
    }

    if (assoc_desc->peer_addresses) {
        int rv = sctp_freepaddrs(assoc_desc->peer_addresses);
        if (rv)
            printf("sctp_freepaddrs(%p) failed\n", assoc_desc->peer_addresses);
    }
    free_wrapper((void **)&assoc_desc);
    sctp_desc.number_of_connections--;
    return 0;
}

//------------------------------------------------------------------------------
static void sctp_dump_assoc(struct sctp_association_s *sctp_assoc_p) {
#if SCTP_DUMP_LIST
    int i;

    if (sctp_assoc_p == NULL) {
        return;
    }

    printf("sd           : %d\n", sctp_assoc_p->sd);
    printf("input streams: %d\n", sctp_assoc_p->instreams);
    printf("out streams  : %d\n", sctp_assoc_p->outstreams);
    printf("assoc_id     : %d\n", sctp_assoc_p->assoc_id);
    printf("peer address :\n");

    for (i = 0; i < sctp_assoc_p->nb_peer_addresses; i++) {
        char address[40];

        memset(address, 0, sizeof(address));

        if (inet_ntop(sctp_assoc_p->peer_addresses[i].sa_family,
                      sctp_assoc_p->peer_addresses[i].sa_data, address,
                      sizeof(address)) != NULL) {
            printf("    - [%s]\n", address);
        }
    }

#else
    sctp_assoc_p = sctp_assoc_p;
#endif
}

//------------------------------------------------------------------------------
static void sctp_dump_list(void) {
#if SCTP_DUMP_LIST
    struct sctp_association_s *sctp_assoc_p =
        sctp_desc.available_connections_head;
    printf("SCTP list contains %d associations\n",
           sctp_desc.number_of_connections);

    while (sctp_assoc_p != NULL) {
        sctp_dump_assoc(sctp_assoc_p);
        sctp_assoc_p = sctp_assoc_p->next_assoc;
    }

#else
    sctp_dump_assoc(NULL);
#endif
}

int server_sctp_send_msg_to_first_assoc(uint16_t stream, const uint8_t *buffer,
                                        const uint32_t length) {
    return server_sctp_send_msg(sctp_desc.available_connections_head->assoc_id,
                                stream, buffer, length);
}

//------------------------------------------------------------------------------
int server_sctp_send_msg(sctp_assoc_id_t sctp_assoc_id, uint16_t stream,
                         const uint8_t *buffer, const uint32_t length) {
    struct sctp_association_s *assoc_desc = NULL;

    // DevAssert(*buffer);

    if ((assoc_desc = sctp_is_assoc_in_list(sctp_assoc_id)) == NULL) {
        printf("This assoc id has not been fount in list (%d)\n",
               sctp_assoc_id);
        return -1;
    }

    if (assoc_desc->sd == -1) {
        /*
         * The socket is invalid may be closed.
         */
        printf("The socket is invalid may be closed (assoc id %d)\n",
               sctp_assoc_id);
        return -1;
    }

    printf("[%d][%d] Sending buffer of %d bytes on stream %d with ppid %d\n",
           assoc_desc->sd, sctp_assoc_id, length, stream, assoc_desc->ppid);

    /*
     * Send message_p on specified stream of the sd association
     */
    if (sctp_sendmsg(assoc_desc->sd, (const void *)buffer, length, NULL, 0,
                     htonl(assoc_desc->ppid), 0, stream, 0, 0) < 0) {
        printf("send: %s:%d", strerror(errno), errno);
        return -1;
    }
    printf("Successfully sent %d bytes on stream %d\n", length, stream);

    assoc_desc->messages_sent++;
    return 0;
}

//------------------------------------------------------------------------------
int sctp_create_new_listener(SctpInit *init_p) {
    struct sctp_event_subscribe event = {0};
    //  struct sockaddr                        *addr = NULL;
    struct sctp_arg_s *sctp_arg_p = NULL;
    uint16_t i = 0, j = 0;
    int sd = 0;
    int used_addresses = 0;

    DevAssert(init_p != NULL);

    if (init_p->ipv4 == 0 && init_p->ipv6 == 0) {
        printf(
            "Illegal IP configuration upper layer should request at"
            "least ipv4 and/or ipv6 config\n");
        return -1;
    }

    if ((used_addresses = init_p->nb_ipv4_addr + init_p->nb_ipv6_addr) == 0) {
        printf("No address provided...\n");
        return -1;
    }

    union {
        struct sockaddr_in addr_v4;
        struct sockaddr_in6 addr_v6;
    } addr[used_addresses];
    memset((void *)addr, 0, sizeof(addr));

    printf("Creating new listen socket on port %u with\n", init_p->port);

    if (init_p->ipv4 == 1) {
        printf("ipv4 addresses:\n");
        for (i = 0; i < init_p->nb_ipv4_addr; i++) {
            addr[i].addr_v4.sin_family = AF_INET;
            addr[i].addr_v4.sin_port = htons(init_p->port);
            addr[i].addr_v4.sin_addr.s_addr = init_p->ipv4_address[i].s_addr;
            char ipv4[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, (void *)&addr[i].addr_v4.sin_addr.s_addr, ipv4,
                      INET_ADDRSTRLEN);
            printf("\t- %s\n", ipv4);
        }
    }

    if (init_p->ipv6 == 1) {
        printf("ipv6 addresses:\n");
        for (j = 0; j < init_p->nb_ipv6_addr; j++) {
            addr[i + j].addr_v6.sin6_family = AF_INET6;
            addr[i + j].addr_v6.sin6_port = htons(init_p->port);
            addr[i + j].addr_v6.sin6_addr = init_p->ipv6_address[j];
            char ipv6[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, (void *)&init_p->ipv6_address[j], ipv6,
                      INET6_ADDRSTRLEN);
            printf("\t- %s\n", ipv6);
        }
    }

    /** Create for each received address a socket. */
    for (int i = 0; i < used_addresses; i++) {
        if ((sd = socket(((struct sockaddr *)&addr[i])->sa_family, SOCK_STREAM,
                         IPPROTO_SCTP)) < 0) {
            printf("socket: %s:%d\n", strerror(errno), errno);
            return -1;
        }

        event.sctp_data_io_event = 1;
        event.sctp_association_event = 1;
        event.sctp_address_event = 1;
        event.sctp_send_failure_event = 1;
        event.sctp_peer_error_event = 1;
        event.sctp_shutdown_event = 1;
        event.sctp_partial_delivery_event = 1;
        // https://forums.centos.org/viewtopic.php?t=12941
        if (setsockopt(sd, IPPROTO_SCTP, SCTP_EVENTS, &event, 8) < 0) {
            printf("setsockopt: %s:%d\n", strerror(errno), errno);
            return -1;
        }

        /*
         * Some pre-bind socket configuration
         */
        if (sctp_set_init_opt(sd, sctp_desc.nb_instreams,
                              sctp_desc.nb_outstreams, 0, 0) < 0) {
            goto err;
        }

        if (sctp_bindx(sd, (struct sockaddr *)&addr[i], 1,
                       SCTP_BINDX_ADD_ADDR) != 0) {
            printf("sctp_bindx: %s:%d\n", strerror(errno), errno);
            return -1;
        }

        if (listen(sd, 5) < 0) {
            printf("listen: %s:%d\n", strerror(errno), errno);
            return -1;
        }

        if ((sctp_arg_p = malloc(sizeof(struct sctp_arg_s))) == NULL) {
            printf("could not allocate memory\n");
            return -1;
        }

        sctp_arg_p->sd = sd;
        sctp_arg_p->ppid = init_p->ppid;

        if (pthread_create(&assoc_thread, NULL, &sctp_receiver_thread,
                           (void *)sctp_arg_p) < 0) {
            printf("pthread_create: %s:%d\n", strerror(errno), errno);
            return -1;
        }
    }
    return sd;
err:
    printf("error has occured\n");
    if (sd != -1) {
        close(sd);
        sd = -1;
    }

    return -1;
}

//------------------------------------------------------------------------------
static inline int sctp_read_from_socket(int sd, int ppid) {
    int flags = 0, n;
    socklen_t from_len = 0;
    struct sctp_sndrcvinfo sinfo = {0};
    struct sockaddr_in6 addr = {0};
    uint8_t buffer[SCTP_RECV_BUFFER_SIZE];

    if (sd < 0) {
        return -1;
    }

    memset((void *)&addr, 0, sizeof(struct sockaddr_in6));
    from_len = (socklen_t)sizeof(struct sockaddr_in6);
    memset((void *)&sinfo, 0, sizeof(struct sctp_sndrcvinfo));
    n = sctp_recvmsg(sd, (void *)buffer, SCTP_RECV_BUFFER_SIZE,
                     (struct sockaddr *)&addr, &from_len, &sinfo, &flags);

    if (n < 0) {
        printf("An error occured during read\n");
        printf("sctp_recvmsg: %s:%d\n", strerror(errno), errno);
        return SCTP_RC_ERROR;
    }

    if (flags & MSG_NOTIFICATION) {
        union sctp_notification *snp = (union sctp_notification *)buffer;

        /*
         * Client deconnection
         */
        if (SCTP_SHUTDOWN_EVENT == snp->sn_header.sn_type) {
            printf("SCTP_SHUTDOWN_EVENT received\n");
            return sctp_handle_com_down(snp->sn_shutdown_event.sse_assoc_id);
        }
        /*
         * Association has changed.
         */
        else if (SCTP_ASSOC_CHANGE == snp->sn_header.sn_type) {
            struct sctp_assoc_change *sctp_assoc_changed;

            sctp_assoc_changed = &snp->sn_assoc_change;
            printf("Client association changed: %d\n",
                   sctp_assoc_changed->sac_state);

            /*
             * New physical association requested by a peer
             */
            switch (sctp_assoc_changed->sac_state) {
                case SCTP_COMM_UP: {
                    struct sctp_association_s *new_association = NULL;

                    sctp_get_sockinfo(sd, NULL, NULL, NULL);
                    printf("New connection\n");

                    if ((new_association = sctp_add_new_peer()) == NULL) {
                        // TODO: handle this case
                        DevMessage("Unexpected error...\n");
                        return SCTP_RC_ERROR;
                    } else {
                        new_association->sd = sd;
                        new_association->ppid = ppid;
                        new_association->instreams =
                            sctp_assoc_changed->sac_inbound_streams;
                        new_association->outstreams =
                            sctp_assoc_changed->sac_outbound_streams;
                        new_association->assoc_id =
                            sctp_assoc_changed->sac_assoc_id;
                        sctp_get_localaddresses(sd, NULL, NULL);
                        sctp_get_peeraddresses(
                            sd, &new_association->peer_addresses,
                            &new_association->nb_peer_addresses);

                        // if (sctp_itti_send_new_association(
                        //         new_association->assoc_id,
                        //         new_association->instreams,
                        //         new_association->outstreams) < 0) {
                        //     printf("Failed to send message to S1AP\n");
                        //     return SCTP_RC_ERROR;
                        // }
                    }
                } break;

                case SCTP_RESTART: {
                    printf("Received SCTP restart for the new connection.\n");
                    /** No separate SCTP INIT will be expected.. */
                    return SCTP_RC_ERROR;
                } break;

                default:
                    break;
            }
        }
    } else {
        /*
         * Data payload received
         */
        struct sctp_association_s *association;

        if ((association = sctp_is_assoc_in_list(sinfo.sinfo_assoc_id)) ==
            NULL) {
            // TODO: handle this case
            return SCTP_RC_ERROR;
        }

        association->messages_recv++;

        if (ntohl(sinfo.sinfo_ppid) != association->ppid) {
            /*
             * Mismatch in Payload Protocol Identifier,
             * * * * may be we received unsollicited traffic from stack other
             * than S1AP.
             */
            printf(
                "Received data from peer with unsollicited PPID %d, "
                "expecting %d\n",
                ntohl(sinfo.sinfo_ppid), association->ppid);
            return SCTP_RC_ERROR;
        }

        printf(
            "[%d][%d] Msg of length %d received from port %u, on stream "
            "%d, PPID %d\n",
            sinfo.sinfo_assoc_id, sd, n, ntohs(addr.sin6_port),
            sinfo.sinfo_stream, ntohl(sinfo.sinfo_ppid));
        // TODO: make a function to send buffer to mme
        (*handle_received_sctp_message)(buffer, (uint32_t)n,
                                        (uint16_t)ntohl(sinfo.sinfo_ppid),
                                        (uint16_t)sinfo.sinfo_stream);

        // sctp_itti_send_new_message_ind(
        //     &payload, sinfo.sinfo_assoc_id, sinfo.sinfo_stream,
        //     association->instreams, association->outstreams);
    }

    printf("SCTP RETURNING!!\n");

    return SCTP_RC_NORMAL_READ;
}

//------------------------------------------------------------------------------
static int sctp_handle_com_down(sctp_assoc_id_t assoc_id) {
    printf("Sending close connection for assoc_id %u\n", assoc_id);

    // if (sctp_itti_send_com_down_ind(assoc_id) < 0) {
    //     printf("Failed to send message to TASK_S1AP\n");
    // }

    if (sctp_remove_assoc_from_list(assoc_id) < 0) {
        printf("Failed to find client in list\n");
    }

    return SCTP_RC_DISCONNECT;
}

//------------------------------------------------------------------------------
void *sctp_receiver_thread(void *args_p) {
    struct sctp_arg_s *sctp_arg_p = NULL;

    /*
     * maximum file descriptor number
     */
    int fdmax, clientsock, i;

    /*
     * master file descriptor list
     */
    fd_set master;

    /*
     * temp file descriptor list for select()
     */
    fd_set read_fds;

    if ((sctp_arg_p = (struct sctp_arg_s *)args_p) == NULL) {
        pthread_exit(NULL);
    }

    /*
     * clear the master and temp sets
     */
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(sctp_arg_p->sd, &master);
    fdmax = sctp_arg_p->sd; /* so far, it's this one */

    while (1) {
        memcpy(&read_fds, &master, sizeof(master));

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            printf("[%d] Select() error: %s", sctp_arg_p->sd, strerror(errno));
            free_wrapper((void **)&args_p);
            pthread_exit(NULL);
        }

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == sctp_arg_p->sd) {
                    /*
                     * There is data to read on listener socket. This means we
                     * have to accept
                     * * * * the connection.
                     */
                    if ((clientsock = accept(sctp_arg_p->sd, NULL, NULL)) < 0) {
                        printf("[%d] accept: %s:%d\n", sctp_arg_p->sd,
                               strerror(errno), errno);
                        free_wrapper((void **)&args_p);
                        pthread_exit(NULL);
                    } else {
                        FD_SET(clientsock, &master); /* add to master set */

                        if (clientsock > fdmax) {
                            /*
                             * keep track of the maximum
                             */
                            fdmax = clientsock;
                        }
                    }
                } else {
                    int ret;

                    /*
                     * Read from socket
                     */
                    ret = sctp_read_from_socket(i, sctp_arg_p->ppid);

                    /*
                     * When the socket is disconnected we have to update
                     * * * * the fd_set.
                     */
                    if (ret == SCTP_RC_DISCONNECT) {
                        /*
                         * Remove the socket from the FD set and update the max
                         * sd
                         */
                        FD_CLR(i, &master);

                        if (i == fdmax) {
                            while (FD_ISSET(fdmax, &master) == false)
                                fdmax -= 1;
                        }
                    }
                }
            }
        }
    }

    free_wrapper((void **)&args_p);
    return NULL;
}

//------------------------------------------------------------------------------
// static void *sctp_intertask_interface(void *args_p) {
//     itti_mark_task_ready(TASK_SCTP);

//     while (1) {
//         MessageDef *received_message_p = NULL;

//         itti_receive_msg(TASK_SCTP, &received_message_p);

//         switch (ITTI_MSG_ID(received_message_p)) {
//             case SCTP_CLOSE_ASSOCIATION: {
//             } break;

//             case SCTP_DATA_REQ: {
//                 if
//                 (server_sctp_send_msg(SCTP_DATA_REQ(received_message_p).assoc_id,
//                                   SCTP_DATA_REQ(received_message_p).stream,
//                                   &SCTP_DATA_REQ(received_message_p).payload)
//                                   <
//                     0) {
//                     sctp_itti_send_lower_layer_conf(
//                         received_message_p->ittiMsgHeader.originTaskId,
//                         SCTP_DATA_REQ(received_message_p).assoc_id,
//                         SCTP_DATA_REQ(received_message_p).stream,
//                         SCTP_DATA_REQ(received_message_p).mme_ue_s1ap_id,
//                         false);
//                 } /* NO NEED FOR CONFIRM success yet else {
//                   if (INVALID_MME_UE_S1AP_ID != SCTP_DATA_REQ
//                 (received_message_p).mme_ue_s1ap_id) {
//                     sctp_itti_send_lower_layer_conf(received_message_p->ittiMsgHeader.originTaskId,
//                         SCTP_DATA_REQ (received_message_p).assoc_id,
//                         SCTP_DATA_REQ (received_message_p).stream,
//                         SCTP_DATA_REQ (received_message_p).mme_ue_s1ap_id,
//                         true);
//                   }
//                 }*/
//             } break;

//             case SCTP_INIT_MSG: {
//                 printf("Received SCTP_INIT_MSG\n");

//                 /*
//                  * We received a new connection request
//                  */
//                 if (sctp_create_new_listener(
//                         &received_message_p->ittiMsg.sctpInit) < 0) {
//                     /*
//                      * SCTP socket creation or bind failed...
//                      */
//                     printf("Failed to create new SCTP listener\n");
//                 }
//             } break;

//             case MESSAGE_TEST: {
//                 printf("TASK_SCTP received MESSAGE_TEST\n");
//             } break;

//             case TERMINATE_MESSAGE: {
//                 sctp_exit();
//                 itti_free_msg_content(received_message_p);
//                 itti_free(ITTI_MSG_ORIGIN_ID(received_message_p),
//                           received_message_p);
//                 itti_exit_task();
//             } break;

//             default: {
//                 printf("Unkwnon message ID %d:%s\n",
//                        ITTI_MSG_ID(received_message_p),
//                        ITTI_MSG_NAME(received_message_p));
//             } break;
//         }

//         itti_free_msg_content(received_message_p);
//         itti_free(ITTI_MSG_ORIGIN_ID(received_message_p),
//         received_message_p); received_message_p = NULL;
//     }

//     return NULL;
// }

//------------------------------------------------------------------------------
int sctp_init(int nb_instreams, int nb_outstreams) {
    printf("Initializing SCTP\n");
    memset(&sctp_desc, 0, sizeof(struct sctp_descriptor_s));
    /*
     * Number of streams from configuration
     */
    sctp_desc.nb_instreams = nb_instreams;
    sctp_desc.nb_outstreams = nb_outstreams;

    // if (itti_create_task(TASK_SCTP, &sctp_intertask_interface, NULL) < 0) {
    //     printf("create task failed");
    //     printf("Initializing SCTP task interface: FAILED\n");
    //     return -1;
    // }

    printf("Initializing SCTP: DONE\n");
    return 0;
}

//------------------------------------------------------------------------------
void sctp_exit(void) {
    int rv = pthread_cancel(assoc_thread);
    if (rv)
        printf("pthread_cancel(%08lX) failed: %d:%s\n", assoc_thread, rv,
               strerror(rv));

    struct sctp_association_s *sctp_assoc_p =
        sctp_desc.available_connections_head;
    struct sctp_association_s *next_sctp_assoc_p =
        sctp_desc.available_connections_head;

    while (next_sctp_assoc_p) {
        next_sctp_assoc_p = sctp_assoc_p->next_assoc;
        if (sctp_assoc_p->peer_addresses) {
            rv = sctp_freepaddrs(sctp_assoc_p->peer_addresses);
            if (rv)
                printf("sctp_freepaddrs(%p) failed\n",
                       sctp_assoc_p->peer_addresses);
        }
        free_wrapper((void **)&sctp_assoc_p);
        sctp_desc.number_of_connections--;
        sctp_assoc_p = next_sctp_assoc_p;
    }
    printf("TASK_SCTP terminated\n");
}
