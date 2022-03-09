# This is Flask OpenID Connect 1.0 configuration
# OpenID Connect 1.0 requires JWT. It can be enabled by setting
OAUTH2_JWT_ENABLED = True

# Algorithm for JWT
OAUTH2_JWT_ALG = 'HS256'

# Private key (in text) for JWT
OAUTH2_JWT_KEY = 'secret-key'

# Issuer value for JWT
OAUTH2_JWT_ISS = 'https://authlib.org'
