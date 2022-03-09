#! /usr/bin/env python3
#
# Python module example file
# Miguel A.L. Paraz <mparaz@mparaz.com>
#
# $Id: 5d437f446e8938beb1d458dc332e4081bf3d5144 $

import radiusd

# Check post_auth for the most complete example using different
# input and output formats

def instantiate(p):
  print("*** instantiate ***")
  print(p)
  # return 0 for success or -1 for failure


def authorize(p):
  print("*** authorize ***")
  radiusd.radlog(radiusd.L_INFO, '*** radlog call in authorize ***')
  print()
  print(p)
  print()
  print(radiusd.config)
  return radiusd.RLM_MODULE_OK

def authenticate(p):
  print("*** authenticate ***")
  radiusd.radlog(radiusd.L_INFO, '*** radlog call in authenticate ***')
  print()
  print(p)
  print()
  print(radiusd.config)

  return radiusd.RLM_MODULE_OK

def preacct(p):
  print("*** preacct ***")
  print(p)
  return radiusd.RLM_MODULE_OK


def accounting(p):
  print("*** accounting ***")
  radiusd.radlog(radiusd.L_INFO, '*** radlog call in accounting (0) ***')
  print()
  print(p)
  return radiusd.RLM_MODULE_OK


def pre_proxy(p):
  print("*** pre_proxy ***")
  print(p)
  return radiusd.RLM_MODULE_OK


def post_proxy(p):
  print("*** post_proxy ***")
  print(p)
  return radiusd.RLM_MODULE_OK


def post_auth(p):
  import os
  print("*** post_auth ***")
  print(p)
  reply = (("Filter-Id", "default.in"), ("Cisco-AVPair", "+=", "url-redirect=http://10.0.2.3:5000/captive"), ("Cisco-AVPair", "+=", "url-redirect-acl=redirect"), )
  if os.path.exists("/src/authenticated"):
    reply = (("Filter-Id", "auth.in"), )
  return radiusd.RLM_MODULE_OK, reply, None


def recv_coa(p):
  print("*** recv_coa ***")
  print(p)
  return radiusd.RLM_MODULE_OK


def send_coa(p):
  print("*** send_coa ***")
  print(p)
  return radiusd.RLM_MODULE_OK


def detach():
  print("*** goodbye from example.py ***")
  return radiusd.RLM_MODULE_OK

