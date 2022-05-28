#ifndef JWT_H
#define JWT_H
#include <Particle.h>
#include <mbedtls/config.h>
#include <mbedtls/check_config.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/base64.h>


struct jwt_ctx_t {
    mbedtls_ecp_group group;
    mbedtls_mpi d;
    mbedtls_sha256_context sha_ctx;
    mbedtls_ecdsa_context ecdsa;
};

int jwt_init(jwt_ctx_t* ctx);
int jwt_gen(jwt_ctx_t* ctx, char* jwt, size_t size, size_t* osize);

#endif