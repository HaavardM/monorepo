#include "jwt.h"
#include <mbedtls/config.h>
#include <mbedtls/check_config.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/base64.h>
#include <Particle.h>

unsigned char private_key[32] = {0x1d, 0x64, 0xb0, 0x67, 0x0b, 0x58, 0x92, 0xbc, 0x4e, 0x76, 0xbf, 0x26, 0x3b, 0x69, 0x63, 0x43, 0x28, 0x0c, 0x68, 0xf4, 0x46, 0x6f, 0x7a, 0xff, 0x4c, 0x51, 0x4d, 0x37, 0xb9, 0xa5, 0x95, 0x88};

int jwt_init(jwt_ctx_t* ctx) {
    int ret = 0;
    mbedtls_ecdsa_init(&ctx->ecdsa);
    mbedtls_ecp_group_init(&ctx->group);
    mbedtls_mpi_init(&ctx->d);
    if ((ret = mbedtls_ecp_group_load(&ctx->group, MBEDTLS_ECP_DP_SECP256R1)) != 0) {
        Serial.printf("group load %X\n", -ret);
        return ret;
    }
    
    if ((ret = mbedtls_mpi_read_binary(&ctx->d, private_key, 32)) != 0) {
        Serial.printf("read_binary %X\n", -ret);
        return ret;
    }
    
    mbedtls_sha256_init(&ctx->sha_ctx);
    return 0;
}

int aes256_hash(jwt_ctx_t* ctx, const unsigned char* buff, size_t n, unsigned char out[32]) {
    int ret = 0;
    
    if ((ret = mbedtls_sha256_starts_ret(&ctx->sha_ctx, 0)) != 0) {
        return ret;
    }
    if ((ret = mbedtls_sha256_update_ret(&ctx->sha_ctx, buff, n)) != 0) {
        return ret;
    }
    return mbedtls_sha256_finish_ret(&ctx->sha_ctx, out);
}

void b64url_from_b64(char* b64, size_t* n) {
    for(size_t i = 0; i < *n; ++i) {
        if (b64[i] == '=') {
            *n = i;
            return;
        }
        if (b64[i] == '+') {
            b64[i] = '-';
        } else if (b64[i] == '/') {
            b64[i] = '_';
        }
    }
}


int jwt_signature(jwt_ctx_t* ctx, const unsigned char* message, size_t n, unsigned char* out, size_t out_len, size_t* alen) {
    int ret = 0;
    unsigned char hash[32];
    mbedtls_mpi r, s;
    size_t curve_bytes, sig_len;

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    if ((ret = aes256_hash(ctx, message, n, hash)) != 0) {
        Serial.printf("Hash error %d\n", ret);
        goto exit;
    }
    if ((ret = mbedtls_ecdsa_sign_det(&ctx->group, &r, &s, &ctx->d, hash, sizeof(hash), MBEDTLS_MD_SHA256)) != 0) {
        Serial.println("ecdsa_sign_det error");
        goto exit;
    }

    unsigned char sig[64];
    curve_bytes = ctx->group.pbits / 8;
    sig_len = 2 * curve_bytes;
    if((ret = mbedtls_mpi_write_binary(&r, sig, curve_bytes)) != 0) {
        Serial.printf("mpi write error %d\n", ret);
        goto exit;
    }
    if((ret = mbedtls_mpi_write_binary(&s, sig + curve_bytes, curve_bytes)) != 0) {
        Serial.printf("mpi write error %d\n", ret);
        goto exit;
    }
    if ((ret = mbedtls_base64_encode(out, out_len, alen, sig, sig_len)) != 0) {
        Serial.println("base64");
        goto exit;
    }
    b64url_from_b64((char*) out, alen);
    ret = 0;
exit:
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    return ret;
}

int jwt_gen(jwt_ctx_t* ctx, char* jwt, size_t size, size_t* osize) {

    int now = Time.now();

    char header[128];
    JSONBufferWriter headerWriter(header, sizeof(header));
    headerWriter.beginObject();
    headerWriter.name("alg").value("ES256");
    headerWriter.name("typ").value("JWT");
    headerWriter.endObject();

    char claims[128];
    JSONBufferWriter claimsWriter(claims, sizeof(claims));
    claimsWriter.beginObject();
    claimsWriter.name("iat").value(now);
    claimsWriter.name("exp").value(now + 23*60*60);
    claimsWriter.name("aud").value("wearebrews");
    claimsWriter.endObject();
    
    char header_b64[128];
    char claims_b64[128];
    size_t header_b64_len, claims_b64_len;

    int ret = 0;
    if (( ret = mbedtls_base64_encode((unsigned char*) header_b64, sizeof(header_b64), &header_b64_len, (unsigned char*) headerWriter.buffer(), headerWriter.dataSize())) != 0) {
        return ret;
    }
    if (( ret = mbedtls_base64_encode( (unsigned char*) claims_b64, sizeof(claims_b64), &claims_b64_len, (unsigned char*) claimsWriter.buffer(), claimsWriter.dataSize())) != 0) {
        return ret;
    }
    b64url_from_b64(header_b64, &header_b64_len);
    b64url_from_b64(claims_b64, &claims_b64_len);

    if (size < header_b64_len + claims_b64_len + 1) {
        return -1;
    }

    size_t jwt_len = 0;
    memcpy(jwt, header_b64, header_b64_len);
    jwt_len += header_b64_len;
    jwt[header_b64_len] = '.';
    jwt_len++;
    memcpy(jwt + header_b64_len + 1, claims_b64, claims_b64_len);
    jwt_len += claims_b64_len;

    char sign[256];
    size_t sign_len;

    if ((ret = jwt_signature(ctx, (unsigned char*) jwt, jwt_len, (unsigned char*) sign, sizeof(sign), &sign_len)) != 0) {
        Serial.println("sign_error");
        return ret;
    }
    if (size < jwt_len + 1 + sign_len) {
        return -1;
    }
    jwt[header_b64_len + claims_b64_len + 1] = '.';
    jwt_len++;
    memcpy(jwt + jwt_len, sign, sign_len);
    jwt_len += sign_len;

    *osize = jwt_len;
    return 0;
}