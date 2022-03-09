# foreign 802.1x

1. User
2. Switch (I have used a Cisco 2960 switch)
3. Foreign AAA (FreeRadius)
4. Virtual AAA (FreeRadius) + Proxy controller

The scenario is as following:
1. User initiates 802.1x authentication with the switch
2. Switch sends an access request to the foreign AAA
3. Foreign AAA proxies the request to virtual AAA (foreign AAA always acts as a proxy between the switch and the virtual AAA)
4. Virtual AAA first sends a request to the internal proxy controller to inform the proxy controller that the user is being authenticated.
4a. The proxy controller initializes the captive portal module (i.e. captive.py)
4b. Then, virtual AAA sends an access accept with URL-Redirect and Filter-ID attributes set so that the user will be redirected to the captive portal. After receiving those attributes, the port will be authenticated but all the requests will be redirected to the captive portal. We use access lists to do this. See the cisco configuration file.
5. User goes to the captive portal which resides at the proxy
6. Proxy logs in on the user's behalf to the home network
7. Proxy sends an RADIUS CoA (Change-of-Authorization) reauthentication packet to the switch through the foreign AAA, creates an "authenticated" file and shows the success screen.
8. Now, during this second authentication, the virtual AAA sees the "authenticated" file and returns back attributes that allow the user full access to the network.


Foreign AAA (IP=10.0.2.2) configurations should be directly usable. I ran FreeRadius with these configurations. Proxy (IP=10.0.2.3) needs to run on a linux machine with Docker Compose. The text file contains the Cisco configuration that I currently have on my switch. The user simply uses a web browser. However, Windows machines have issues with EAP-TTLS (we need to define a certificate authority etc. etc.) so you may want to use a mac or linux machine as the user. 
