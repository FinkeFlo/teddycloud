#include <errno.h>
#include <sys/random.h>

#include "pem_export.h"
#include "rng/yarrow.h"
#include "error.h"
#include "debug.h"

#define APP_CA_CERT_BUNDLE "certs/ca.der"
#define APP_CLIENT_CERT "certs/client.der"
#define APP_CLIENT_PRIVATE_KEY "certs/private.der"

char_t *clientCert = NULL;
size_t clientCertLen = 0;
char_t *clientPrivateKey = NULL;
size_t clientPrivateKeyLen = 0;
char_t *trustedCaList = NULL;
size_t trustedCaListLen = 0;
YarrowContext yarrowContext;

/**
 * @brief Load the specified PEM file
 * @param[in] filename Name of the PEM file to load
 * @param[out] buffer Memory buffer that holds the contents of the file
 * @param[out] length Length of the file in bytes
 **/
error_t readPemFile(const char_t *filename, char_t **buffer, size_t *length, const char_t *type)
{
    int_t ret;
    error_t error;
    FILE *fp;

    // Initialize output parameters
    *buffer = NULL;
    *length = 0;

    // Start of exception handling block
    do
    {
        // Open the specified file
        fp = fopen(filename, "rb");

        // Failed to open the file?
        if (fp == NULL)
        {
            error = ERROR_OPEN_FAILED;
            break;
        }

        // Jump to the end of the file
        ret = fseek(fp, 0, SEEK_END);

        // Any error to report?
        if (ret != 0)
        {
            error = ERROR_FAILURE;
            break;
        }

        // Retrieve the length of the file
        *length = ftell(fp);
        // Allocate a buffer to hold the contents of the file
        *buffer = malloc(*length);

        // Failed to allocate memory?
        if (*buffer == NULL)
        {
            error = ERROR_OUT_OF_MEMORY;
            break;
        }

        // Rewind to the beginning of the file
        rewind(fp);
        // Read file contents
        ret = fread(*buffer, 1, *length, fp);

        // Failed to read data?
        if (ret != *length)
        {
            error = ERROR_READ_FAILED;
            break;
        }

        // Successful processing
        error = NO_ERROR;

        // End of exception handling block
    } while (0);

    // Close file
    if (fp != NULL)
        fclose(fp);

    // Any error to report?
    if (error)
    {
        // Debug message
        TRACE_ERROR("Error: Cannot load file %s\r\n", filename);
        // Clean up side effects
        free(*buffer);
    }

    /* convert .der to .pem by encoding it into ascii format */
    if (type)
    {
        char *inBuf = *buffer;
        size_t inBufLen = *length;

        char *outBuf = NULL;
        size_t outBufLen = 0;

        /* get size of output string */
        error = pemEncodeFile(inBuf, inBufLen, type, NULL, &outBufLen);

        if (error != NO_ERROR)
        {
            TRACE_ERROR("Error: pemEncodeFile failed for %s with code %d\r\n", filename, error);
            return error;
        }

        outBuf = malloc(outBufLen + 1);
        error = pemEncodeFile(inBuf, inBufLen, type, outBuf, &outBufLen);

        free(inBuf);

        *buffer = outBuf;
        *length = outBufLen;
    }

    // Return status code
    return error;
}

error_t tls_adapter_deinit()
{
    // Free previously allocated resources
    free(trustedCaList);
    free(clientCert);
    free(clientPrivateKey);

    // Release PRNG context
    yarrowRelease(&yarrowContext);

    return NO_ERROR;
}

error_t tls_adapter_init()
{
    uint8_t seed[32];

    int ret = getrandom(seed, sizeof(seed), GRND_RANDOM);
    if (ret < 0)
    {
        TRACE_ERROR("Error: Failed to generate random data (%d)\r\n", errno);
        return ERROR_FAILURE;
    }

    error_t error = yarrowInit(&yarrowContext);
    if (error)
    {
        TRACE_ERROR("Error: PRNG initialization failed (%d)\r\n", error);
        return ERROR_FAILURE;
    }

    error = yarrowSeed(&yarrowContext, seed, sizeof(seed));
    if (error)
    {
        TRACE_ERROR("Error: Failed to seed PRNG (%d)\r\n", error);
        return error;
    }

    TRACE_INFO("Loading certificates...\r\n");

    // Load trusted CA certificates
    error = readPemFile(APP_CA_CERT_BUNDLE, &trustedCaList,
                        &trustedCaListLen, "CERTIFICATE");
    if (error)
        return error;

    // Load client's certificate
    error = readPemFile(APP_CLIENT_CERT, &clientCert, &clientCertLen, "CERTIFICATE");
    if (error)
        return error;

    // Load client's private key
    error = readPemFile(APP_CLIENT_PRIVATE_KEY, &clientPrivateKey,
                        &clientPrivateKeyLen, "RSA PRIVATE KEY");
    if (error)
        return error;

    return NO_ERROR;
}
