/**
 * \file error.h
 *
 * Defines error numbers and functions to translate those to a readable string.
 *
 */
 
/**
 * a Net::DNS like library for C
 *
 * (c) NLnet Labs, 2005-2006
 *
 * See the file LICENSE for the license
 */

#ifndef LDNS_ERROR_H
#define LDNS_ERROR_H

#include <ldns/util.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ldns_enum_status {
	LDNS_STATUS_OK,	
	LDNS_STATUS_EMPTY_LABEL,
	LDNS_STATUS_LABEL_OVERFLOW,
	LDNS_STATUS_DOMAINNAME_OVERFLOW,
	LDNS_STATUS_DOMAINNAME_UNDERFLOW,
	LDNS_STATUS_DDD_OVERFLOW,
	LDNS_STATUS_PACKET_OVERFLOW,
	LDNS_STATUS_INVALID_POINTER,
	LDNS_STATUS_MEM_ERR,
	LDNS_STATUS_INTERNAL_ERR,
	LDNS_STATUS_SSL_ERR,
	LDNS_STATUS_ERR,
	LDNS_STATUS_INVALID_INT,
	LDNS_STATUS_INVALID_IP4,
	LDNS_STATUS_INVALID_IP6,
	LDNS_STATUS_INVALID_STR,
	LDNS_STATUS_INVALID_B32_EXT,
	LDNS_STATUS_INVALID_B64,
	LDNS_STATUS_INVALID_HEX,
	LDNS_STATUS_INVALID_TIME,
	LDNS_STATUS_NETWORK_ERR,
	LDNS_STATUS_ADDRESS_ERR,
	LDNS_STATUS_FILE_ERR,
	LDNS_STATUS_UNKNOWN_INET,
	LDNS_STATUS_NOT_IMPL,
	LDNS_STATUS_NULL,
	LDNS_STATUS_CRYPTO_UNKNOWN_ALGO, 
	LDNS_STATUS_CRYPTO_ALGO_NOT_IMPL, 	
	LDNS_STATUS_CRYPTO_NO_RRSIG,
	LDNS_STATUS_CRYPTO_NO_DNSKEY,
	LDNS_STATUS_CRYPTO_NO_TRUSTED_DNSKEY,
	LDNS_STATUS_CRYPTO_NO_DS,
	LDNS_STATUS_CRYPTO_NO_TRUSTED_DS,
	LDNS_STATUS_CRYPTO_NO_MATCHING_KEYTAG_DNSKEY,
	LDNS_STATUS_CRYPTO_VALIDATED,
	LDNS_STATUS_CRYPTO_BOGUS,
	LDNS_STATUS_CRYPTO_SIG_EXPIRED,
	LDNS_STATUS_CRYPTO_SIG_NOT_INCEPTED,
	LDNS_STATUS_CRYPTO_TSIG_BOGUS,
	LDNS_STATUS_CRYPTO_TSIG_ERR,
	LDNS_STATUS_CRYPTO_EXPIRATION_BEFORE_INCEPTION,
	LDNS_STATUS_CRYPTO_TYPE_COVERED_ERR,
	LDNS_STATUS_ENGINE_KEY_NOT_LOADED,
	LDNS_STATUS_NSEC3_ERR,
	LDNS_STATUS_RES_NO_NS,
	LDNS_STATUS_RES_QUERY,
	LDNS_STATUS_WIRE_INCOMPLETE_HEADER,
	LDNS_STATUS_WIRE_INCOMPLETE_QUESTION,
	LDNS_STATUS_WIRE_INCOMPLETE_ANSWER,
	LDNS_STATUS_WIRE_INCOMPLETE_AUTHORITY,
	LDNS_STATUS_WIRE_INCOMPLETE_ADDITIONAL,
	LDNS_STATUS_NO_DATA,
	LDNS_STATUS_CERT_BAD_ALGORITHM,
	LDNS_STATUS_SYNTAX_TYPE_ERR,
	LDNS_STATUS_SYNTAX_CLASS_ERR,
	LDNS_STATUS_SYNTAX_TTL_ERR,
	LDNS_STATUS_SYNTAX_INCLUDE_ERR_NOTIMPL,
	LDNS_STATUS_SYNTAX_RDATA_ERR,
	LDNS_STATUS_SYNTAX_DNAME_ERR,
	LDNS_STATUS_SYNTAX_VERSION_ERR,
	LDNS_STATUS_SYNTAX_ALG_ERR,
	LDNS_STATUS_SYNTAX_KEYWORD_ERR,
	LDNS_STATUS_SYNTAX_TTL,
	LDNS_STATUS_SYNTAX_ORIGIN,
	LDNS_STATUS_SYNTAX_INCLUDE,
	LDNS_STATUS_SYNTAX_EMPTY,
	LDNS_STATUS_SYNTAX_ITERATIONS_OVERFLOW,
	LDNS_STATUS_SYNTAX_MISSING_VALUE_ERR,
	LDNS_STATUS_SYNTAX_INTEGER_OVERFLOW,
	LDNS_STATUS_SYNTAX_BAD_ESCAPE,
	LDNS_STATUS_SOCKET_ERROR,
	LDNS_STATUS_SYNTAX_ERR,
	LDNS_STATUS_DNSSEC_EXISTENCE_DENIED,
	LDNS_STATUS_DNSSEC_NSEC_RR_NOT_COVERED,
	LDNS_STATUS_DNSSEC_NSEC_WILDCARD_NOT_COVERED,
	LDNS_STATUS_DNSSEC_NSEC3_ORIGINAL_NOT_FOUND,
	LDNS_STATUS_MISSING_RDATA_FIELDS_RRSIG,
	LDNS_STATUS_MISSING_RDATA_FIELDS_KEY,
	LDNS_STATUS_CRYPTO_SIG_EXPIRED_WITHIN_MARGIN,
	LDNS_STATUS_CRYPTO_SIG_NOT_INCEPTED_WITHIN_MARGIN,
	LDNS_STATUS_DANE_STATUS_MESSAGES,
	LDNS_STATUS_DANE_UNKNOWN_CERTIFICATE_USAGE,
	LDNS_STATUS_DANE_UNKNOWN_SELECTOR,
	LDNS_STATUS_DANE_UNKNOWN_MATCHING_TYPE,
	LDNS_STATUS_DANE_UNKNOWN_PROTOCOL,
	LDNS_STATUS_DANE_UNKNOWN_TRANSPORT,
	LDNS_STATUS_DANE_MISSING_EXTRA_CERTS,
	LDNS_STATUS_DANE_EXTRA_CERTS_NOT_USED,
	LDNS_STATUS_DANE_OFFSET_OUT_OF_RANGE,
	LDNS_STATUS_DANE_INSECURE,
	LDNS_STATUS_DANE_BOGUS,
	LDNS_STATUS_DANE_TLSA_DID_NOT_MATCH,
	LDNS_STATUS_DANE_NON_CA_CERTIFICATE,
	LDNS_STATUS_DANE_PKIX_DID_NOT_VALIDATE,
	LDNS_STATUS_DANE_PKIX_NO_SELF_SIGNED_TRUST_ANCHOR,
	LDNS_STATUS_EXISTS_ERR,
	LDNS_STATUS_INVALID_ILNP64,
	LDNS_STATUS_INVALID_EUI48,
	LDNS_STATUS_INVALID_EUI64,
	LDNS_STATUS_WIRE_RDATA_ERR,
	LDNS_STATUS_INVALID_TAG,
	LDNS_STATUS_TYPE_NOT_IN_BITMAP,
	LDNS_STATUS_INVALID_RDF_TYPE,
	LDNS_STATUS_RDATA_OVERFLOW,
	LDNS_STATUS_SYNTAX_SUPERFLUOUS_TEXT_ERR,
	LDNS_STATUS_NSEC3_DOMAINNAME_OVERFLOW,
	LDNS_STATUS_DANE_NEED_OPENSSL_GE_1_1_FOR_DANE_TA,
	LDNS_STATUS_ZONEMD_UNKNOWN_SCHEME,
	LDNS_STATUS_ZONEMD_UNKNOWN_HASH,
	LDNS_STATUS_ZONEMD_INVALID_SOA,
	LDNS_STATUS_NO_ZONEMD,
	LDNS_STATUS_NO_VALID_ZONEMD
};
typedef enum ldns_enum_status ldns_status;

extern ldns_lookup_table ldns_error_str[];

/**
 * look up a descriptive text by each error. This function
 * could use a better name
 * \param[in] err ldns_status number
 * \return the string for that error
 */
const char *ldns_get_errorstr_by_id(ldns_status err);

#ifdef __cplusplus
}
#endif

#endif /* LDNS_ERROR_H */
