/*
 * Verify or create TLS authentication with DANE (RFC6698)
 *
 * (c) NLnetLabs 2012
 *
 * See the file LICENSE for the license.
 *
 * TODO before release:
 * 
 * - sigchase
 * - trace up from root
 *
 * Long term wishlist:
 * - Interact with user after connect
 */

#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <ldns/ldns.h>

#include <errno.h>

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#define LDNS_ERR(code, msg) do { if (code != LDNS_STATUS_OK) \
					ldns_err(msg, code); } while(0)
#define MEMERR(msg) do { fprintf(stderr, "memory error in %s\n", msg); \
			 exit(EXIT_FAILURE); } while(0)

void
print_usage(const char* progname)
{
	printf("Usage: %s [OPTIONS] <name> <port>\n", progname);
	printf("\n\tMake a TLS connection to <name> <port> "
			"and use the TLSA\n\t"
			"resource record(s) at <name> to authenticate "
			"the connection.\n");
	printf("\n  or: %s [OPTIONS] <name> <port> <cert usage> <selector> "
			"<matching type>\n", progname);
	printf("\n\tMake a TLS connection to <name> <port> "
			"and create the TLSA\n\t"
			"resource record(s) that would "
			"authenticate the connection.\n");
	printf("\n\t<certificate usage>"
			"\t0: CA constraint\n"
			"\t\t\t\t1: Service certificate constraint\n"
			"\t\t\t\t2: Trust anchor assertion\n"
			"\t\t\t\t3: Domain-issued certificate\n");
	printf("\n\t<selector>"
			"\t\t0: Full certificate\n"
			"\t\t\t\t1: SubjectPublicKeyInfo\n");
	printf("\n\t<matching type>"
			"\t\t0: No hash used\n"
			"\t\t\t\t1: SHA-256\n"
			"\t\t\t\t2: SHA-512\n");

	printf("\nOPTIONS:\n");
	printf("\t-h\t\tshow this text\n\n");
	printf("\t-4\t\tTLS connect IPv4 only\n");
	printf("\t-6\t\tTLS connect IPv6 only\n\n");
	printf("\t-a <address>\t"
	       "Don't try to resolve <name>, but connect to <address>\n"
	       "\t\t\tin stead.\n"
	       "\t\t\tThis option may be given more than once.\n"
	       "\n"
	      );
	printf("\t-b\t\t"
	       "print \"<name>. TYPE52 \\#<size> <hex data>\" form\n"
	       "\t\t\tin stead of TLSA presentation format.\n"
	       "\n"
	      );
	printf("\t-c <file>\t"
	       "do not TLS connect to <name> <port>,\n"
	       "\t\t\tbut authenticate (or make TLSA records)\n"
	       "\t\t\tfor the certificate (chain) in <file> in stead\n"
	       "\n"
	      );
	printf("\t-d\t\tassume DNSSEC validity even when insecure\n\n");
	printf("\t-f <CAfile>\tuse CAfile to validate\n\n");
	printf("\t-i <number>\t"
	       "When creating a \"Trust anchor assertion\" TLSA resource\n"
	       "\t\t\trecord, select the <number>th certificate from the\n"
	       "\t\t\tthe validation chain. Where 0 means the last\n"
	       "\t\t\tcertificate, 1 the one but last, etc.\n"
	       "\n"
	       "\t\t\tWhen <number> is -1, the last certificate is used\n"
	       "\t\t\t(like with 0) that MUST be self-signed. This can help\n"
	       "\t\t\tto make sure that the intended (self signed) trust\n"
	       "\t\t\tanchor is actually present in the server certificate\n"
	       "\t\t\tchain (which is a DANE requirement)\n"
	       "\n"
	      );
	printf("\t-p <CApath>\t"
	       "use certificates in the <CApath> directory to validate\n"
	       "\n"
	      );
	printf("\t-k <file>\t"
	       "specify a file that contains a trusted DNSKEY or DS rr.\n"
	       "\t\t\tWithout a trusted DNSKEY, the local network is trusted\n"
	       "\t\t\tto provide a DNSSEC resolver (i.e. AD bit is checked).\n"
	       "\n"
	       "\t\t\tWhen -r <file> is also given, DNSSEC validation is\n"
	       "\t\t\t\"traced\" from the root down. With only -k <file> and\n"
	       "\t\t\tno root hints, signature(s) are chased to a known key.\n"
	       "\n"
	       "\t\t\tThis option may be given more than once.\n"
	       "\n"
	      );
	printf("\t-n\t\tDo *not* verify server name in certificate\n\n");
	printf("\t-r <file>\tuse <file> to read root hints from\n\n");
	printf("\t-s\t\twhen creating TLSA resource records with the\n\t\t\t"
	       "\"CA Constraint\" and the \"Service Certificate\n\t\t\t"
	       "Constraint\" certificate usage, do not validate and\n\t\t\t"
	       "assume PKIX is valid.\n\n\t\t\t"
	       "For \"CA Constraint\" this means that verification\n\t\t\t"
	       "should end with a self-signed certificate.\n\n");
	printf("\t-u\t\tuse UDP in stead of TCP to TLS connect\n\n");
	exit(EXIT_SUCCESS);
}

int
usage_within_range(const char* arg, int max, const char* name)
{
	char* endptr; /* utility var for strtol usage */
	int val = strtol(arg, &endptr, 10);

	if ((val < 0 || val > max)
			|| (errno != 0 && val == 0) /* out of range */
			|| endptr == arg            /* no digits */
			|| *endptr != '\0'          /* more chars */
			) {
		fprintf(stderr, "<%s> should be in range [0-%d]\n", name, max);
		exit(EXIT_FAILURE);
	}
	return val;
}

void
ssl_err(const char* s)
{
	fprintf(stderr, "error: %s\n", s);
	ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
}

void
ldns_err(const char* s, ldns_status err)
{
	if (err == LDNS_STATUS_SSL_ERR) {
		ssl_err(s);
	} else {
		fprintf(stderr, "error: %s\n", ldns_get_errorstr_by_id(err));
		exit(EXIT_FAILURE);
	}
}

ldns_status
get_ssl_cert_chain(X509** cert, STACK_OF(X509)** extra_certs, SSL* ssl,
		ldns_rdf* address, uint16_t port,
		ldns_dane_transport transport)
{
	struct sockaddr_storage *a = NULL;
	size_t a_len = 0;
	int sock;
	int r;
	ldns_status s;

	assert(cert != NULL);
	assert(extra_certs != NULL);

	a = ldns_rdf2native_sockaddr_storage(address, port, &a_len);
	switch (transport) {
	case LDNS_DANE_TRANSPORT_TCP:

		sock = socket((int)((struct sockaddr*)a)->sa_family,
				SOCK_STREAM, IPPROTO_TCP);
		break;

	case LDNS_DANE_TRANSPORT_UDP:

		sock = socket((int)((struct sockaddr*)a)->sa_family,
				SOCK_DGRAM, IPPROTO_UDP);
		break;

	case LDNS_DANE_TRANSPORT_SCTP:

		sock = socket((int)((struct sockaddr*)a)->sa_family,
				SOCK_STREAM, IPPROTO_SCTP);
		break;

	default:
		LDNS_FREE(a);
		s = LDNS_STATUS_DANE_UNKNOWN_TRANSPORT;
		goto error;
	}
	if (sock == -1) {
		s = LDNS_STATUS_NETWORK_ERR;
		goto error;
	}
	if (connect(sock, (struct sockaddr*)a, (socklen_t)a_len) == -1) {
		s = LDNS_STATUS_NETWORK_ERR;
		goto error;
	}
	if (! SSL_clear(ssl)) {
		close(sock);
		fprintf(stderr, "SSL_clear\n");
		s = LDNS_STATUS_SSL_ERR;
		goto error;
	}
	SSL_set_connect_state(ssl);
	(void) SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
	if (! SSL_set_fd(ssl, sock)) {
		close(sock);
		fprintf(stderr, "SSL_set_fd\n");
		s = LDNS_STATUS_SSL_ERR;
		goto error;
	}
	while (1) {
		ERR_clear_error();
		if ((r = SSL_do_handshake(ssl)) == 1) {
			break;
		}
		r = SSL_get_error(ssl, r);
		if (r != SSL_ERROR_WANT_READ && r != SSL_ERROR_WANT_WRITE) {
			fprintf(stderr, "SSL_get_error unknown return code\n");
			s = LDNS_STATUS_SSL_ERR;
			goto error;
		}
	}
	*cert = SSL_get_peer_certificate(ssl);
	*extra_certs = SSL_get_peer_cert_chain(ssl);

	/*
	 * TODO: feature request: interact with user here
	 */
	while ((r = SSL_shutdown(ssl)) == 0)
		;

	/* TODO: SSL_shutdown errors with www.google.com:443. Why?
	 *       SSL_get_error(ssl, r) gives SSL_ERROR_SYSCALL, but errno == 0
	 *       and there are no messages on the SSL error stack!
	 
	if (r == -1) {
		fprintf(stderr, "SSL_shutdown\n");
		r = SSL_get_error(ssl, r);
		fprintf(stderr, "SSL_get_error return code: %d\n", r);
		s = LDNS_STATUS_SSL_ERR;
		goto error;
	}
	*/

	LDNS_FREE(a);
	return LDNS_STATUS_OK;
error:
	LDNS_FREE(a);
	return s;
}

ldns_rr_list*
rr_list_filter_rr_type(ldns_rr_list* l, ldns_rr_type t)
{
	size_t i;
	ldns_rr* rr;
	ldns_rr_list* r = ldns_rr_list_new();

	if (r == NULL) {
		return r;
	}
	for (i = 0; i < ldns_rr_list_rr_count(l); i++) {
		rr = ldns_rr_list_rr(l, i);
		if (ldns_rr_get_type(rr) == t) {
			if (! ldns_rr_list_push_rr(r, rr)) {
				ldns_rr_list_free(r);
				return NULL;
			}
		}
	}
	return r;
}


ldns_rr_list*
dane_no_pkix_transform(const ldns_rr_list* tlas)
{
	size_t i;
	ldns_rr* rr;
	ldns_rr* new_rr;
	ldns_rdf* rdf;
	ldns_rr_list* r = ldns_rr_list_new();

	if (r == NULL) {
		return r;
	}
	for (i = 0; i < ldns_rr_list_rr_count(tlas); i++) {
		rr = ldns_rr_list_rr(tlas, i);
		if (ldns_rr_get_type(rr) == LDNS_RR_TYPE_TLSA) {

			new_rr = ldns_rr_clone(rr);
			if (!new_rr) {
				ldns_rr_list_deep_free(r);
				return NULL;
			}
			switch(ldns_rdf2native_int8(ldns_rr_rdf(new_rr, 0))) {

			case LDNS_TLSA_USAGE_CA_CONSTRAINT:

				rdf = ldns_native2rdf_int8(LDNS_RDF_TYPE_INT8,
			(uint8_t) LDNS_TLSA_USAGE_TRUST_ANCHOR_ASSERTION);
				if (! rdf) {
					ldns_rr_free(new_rr);
					ldns_rr_list_deep_free(r);
					return NULL;
				}
				(void) ldns_rr_set_rdf(new_rr, rdf, 0);
				break;


			case LDNS_TLSA_USAGE_SERVICE_CERTIFICATE_CONSTRAINT:

				rdf = ldns_native2rdf_int8(LDNS_RDF_TYPE_INT8,
			(uint8_t) LDNS_TLSA_USAGE_DOMAIN_ISSUED_CERTIFICATE);
				if (! rdf) {
					ldns_rr_free(new_rr);
					ldns_rr_list_deep_free(r);
					return NULL;
				}
				(void) ldns_rr_set_rdf(new_rr, rdf, 0);
				break;


			default:
				break;
			}
			if (! ldns_rr_list_push_rr(r, new_rr)) {
				ldns_rr_free(new_rr);
				ldns_rr_list_deep_free(r);
				return NULL;
			}
		}
	}
	return r;
}

void
print_rr_as_TYPEXXX(FILE* out, ldns_rr* rr)
{
	size_t i, sz;
	ldns_status s;
	ldns_buffer* buf = ldns_buffer_new(LDNS_MAX_PACKETLEN);
	char* str;

	ldns_buffer_clear(buf);
	s = ldns_rdf2buffer_str_dname(buf, ldns_rr_owner(rr));
	LDNS_ERR(s, "could not ldns_rdf2buffer_str_dname");
	ldns_buffer_printf(buf, "\t%d", ldns_rr_ttl(rr));
	ldns_buffer_printf(buf, "\t");
	s = ldns_rr_class2buffer_str(buf, ldns_rr_get_class(rr));
	LDNS_ERR(s, "could not ldns_rr_class2buffer_str");
	ldns_buffer_printf(buf, "\tTYPE%d", ldns_rr_get_type(rr));
	sz = 0;
	for (i = 0; i < ldns_rr_rd_count(rr); i++) {
		 sz += ldns_rdf_size(ldns_rr_rdf(rr, i));
	}
	ldns_buffer_printf(buf, "\t\\#%d ", sz);
	for (i = 0; i < ldns_rr_rd_count(rr); i++) {
		s = ldns_rdf2buffer_str_hex(buf, ldns_rr_rdf(rr, i));
		LDNS_ERR(s, "could not ldns_rdf2buffer_str_hex");
	}
	str = ldns_buffer_export2str(buf);
	ldns_buffer_free(buf);
	fprintf(out, "%s\n", str);
	LDNS_FREE(str);
}

void
print_rr_list_as_TYPEXXX(FILE* out, ldns_rr_list* l)
{
	size_t i;

	for (i = 0; i < ldns_rr_list_rr_count(l); i++) {
		print_rr_as_TYPEXXX(out, ldns_rr_list_rr(l, i));
	}
}

ldns_status
read_key_file(const char *filename, ldns_rr_list *keys)
{
	ldns_status status = LDNS_STATUS_ERR;
	ldns_rr *rr;
	FILE *fp;
	uint32_t my_ttl = 0;
	ldns_rdf *my_origin = NULL;
	ldns_rdf *my_prev = NULL;
	int line_nr;

	if (!(fp = fopen(filename, "r"))) {
		fprintf(stderr, "Error opening %s: %s\n", filename,
				strerror(errno));
		return LDNS_STATUS_FILE_ERR;
	}
	while (!feof(fp)) {
		status = ldns_rr_new_frm_fp_l(&rr, fp, &my_ttl, &my_origin,
				&my_prev, &line_nr);

		if (status == LDNS_STATUS_OK) {

			if (   ldns_rr_get_type(rr) == LDNS_RR_TYPE_DS
			    || ldns_rr_get_type(rr) == LDNS_RR_TYPE_DNSKEY)

				ldns_rr_list_push_rr(keys, rr);

		} else if (   status == LDNS_STATUS_SYNTAX_EMPTY
		           || status == LDNS_STATUS_SYNTAX_TTL
		           || status == LDNS_STATUS_SYNTAX_ORIGIN
		           || status == LDNS_STATUS_SYNTAX_INCLUDE)

			status = LDNS_STATUS_OK;
		else
			break;
	}
	fclose(fp);
	return status;
}


/*
 * The file with the given path should contain a list of NS RRs
 * for the root zone and A records for those NS RRs.
 * Read them, check them, and append the a records to the rr list given.
 */
ldns_rr_list *
read_root_hints(const char *filename)
{
	FILE *fp = NULL;
	int line_nr = 0;
	ldns_zone *z;
	ldns_status status;
	ldns_rr_list *addresses = NULL;
	ldns_rr *rr;
	size_t i;

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "Unable to open %s for reading: %s\n", filename, strerror(errno));
		return NULL;
	}

	status = ldns_zone_new_frm_fp_l(&z, fp, NULL, 0, 0, &line_nr);
	fclose(fp);
	if (status != LDNS_STATUS_OK) {
		fprintf(stderr, "Error reading root hints file: %s\n", ldns_get_errorstr_by_id(status));
		return NULL;
	} else {
		addresses = ldns_rr_list_new();
		for (i = 0; i < ldns_rr_list_rr_count(ldns_zone_rrs(z)); i++) { 
			rr = ldns_rr_list_rr(ldns_zone_rrs(z), i);
			/*
			if ((address_family == 0 || address_family == 1) &&
			    ldns_rr_get_type(rr) == LDNS_RR_TYPE_A ) {
				ldns_rr_list_push_rr(addresses, ldns_rr_clone(rr));
			}
			if ((address_family == 0 || address_family == 2) &&
			    ldns_rr_get_type(rr) == LDNS_RR_TYPE_AAAA) {
				ldns_rr_list_push_rr(addresses, ldns_rr_clone(rr));
			}
			*/
			ldns_rr_list_push_rr(addresses, ldns_rr_clone(rr));
		}
		ldns_zone_deep_free(z);
		return addresses;
	}
}


ldns_status
dane_setup_resolver(ldns_resolver** res,
		ldns_rr_list* keys, ldns_rr_list* dns_root,
		bool dnssec_off)
{
	ldns_status s;

	assert(res != NULL);

	s = ldns_resolver_new_frm_file(res, NULL);
	if (s == LDNS_STATUS_OK) {
		ldns_resolver_set_dnssec(*res, ! dnssec_off);

		/* anchors must trigger signature chasing */
		ldns_resolver_set_dnssec_anchors(*res, keys);

		if (dns_root) {
			if (ldns_resolver_nameserver_count(*res) > 0) {
				free(ldns_resolver_nameservers(*res));
				free(ldns_resolver_rtt(*res));
				ldns_resolver_set_nameserver_count(*res, 0);
				ldns_resolver_set_nameservers(*res, NULL);
				ldns_resolver_set_rtt(*res, NULL);
			}
			s = ldns_resolver_push_nameserver_rr_list(*res,
					dns_root);

			/* recursive set to false will trigger tracing */
			ldns_resolver_set_recursive(*res, false);
		}
	}
	return s;
}


ldns_status
dane_query(ldns_rr_list** rrs, ldns_resolver* r,
		ldns_rdf *name, ldns_rr_type t, ldns_rr_class c,
		bool insecure_is_ok)
{
	ldns_pkt* p;

	assert(rrs != NULL);

	p = ldns_resolver_query(r, name, t, c, LDNS_RD);
	if (! p) {
		ldns_err("ldns_resolver_query", LDNS_STATUS_MEM_ERR);
		return LDNS_STATUS_MEM_ERR;
	}
	*rrs = ldns_pkt_rr_list_by_type(p, t, LDNS_SECTION_ANSWER);
	if (ldns_rr_list_rr_count(*rrs) > 0 &&
			ldns_resolver_dnssec(r) && ! ldns_pkt_ad(p) &&
			! insecure_is_ok) {
		ldns_pkt_free(p);
		if (! insecure_is_ok) {
			ldns_rr_list_deep_free(*rrs);
			*rrs = NULL;
		}
		return LDNS_STATUS_DANE_INSECURE;
	}
	ldns_pkt_free(p);
	return LDNS_STATUS_OK;
}


ldns_rr_list*
dane_lookup_addresses(ldns_resolver* res, ldns_rdf* dname,
		int ai_family)
{
	ldns_status s;
	ldns_rr_list *as = NULL;
	ldns_rr_list *aaas = NULL;
	ldns_rr_list *r = ldns_rr_list_new();

	if (r == NULL) {
		MEMERR("ldns_rr_list_new");
	}
	if (ai_family == AF_UNSPEC || ai_family == AF_INET) {

		s = dane_query(&as, res,
				dname, LDNS_RR_TYPE_A, LDNS_RR_CLASS_IN,
			       	true);

		if (s == LDNS_STATUS_DANE_INSECURE &&
			ldns_rr_list_rr_count(as) > 0) {
			fprintf(stderr, "Warning! Insecure IPv4 addresses\n");

		} else if (s != LDNS_STATUS_OK) {
			LDNS_ERR(s, "dane_query");

		} else if (! ldns_rr_list_push_rr_list(r, as)) {
			MEMERR("ldns_rr_list_push_rr_list");
		}
	}
	if (ai_family == AF_UNSPEC || ai_family == AF_INET6) {

		s = dane_query(&aaas, res,
				dname, LDNS_RR_TYPE_AAAA, LDNS_RR_CLASS_IN,
			       	true);

		if (s == LDNS_STATUS_DANE_INSECURE &&
			ldns_rr_list_rr_count(aaas) > 0) {
			fprintf(stderr, "Warning! Insecure IPv6 addresses\n");

		} else if (s != LDNS_STATUS_OK) {
			LDNS_ERR(s, "dane_query");

		} else if (! ldns_rr_list_push_rr_list(r, aaas)) {
			MEMERR("ldns_rr_list_push_rr_list");
		}
	}
	return r;
}

bool
dane_wildcard_label_cmp(uint8_t iw, const char* w, uint8_t il, const char* l)
{
	if (iw == 0) { /* End of match label */
		if (il == 0) { /* And end in the to be matched label */
			return true;
		}
		return false;
	}
	do {
		if (*w == '*') {
			if (iw == 1) { /* '*' is the last match char,
					  remainder matches wildcard */
				return true;
			}
			while (il > 0) { /* more to match? */

				if (w[1] == *l) { /* Char after '*' matches.
						   * Recursion for backtracking
						   */
					if (dane_wildcard_label_cmp(
								iw - 1, w + 1,
								il    , l)) {
						return true;
					}
				}
				l += 1;
				il -= 1;
			}
		}
		/* Skip up till next wildcard (if possible) */
		while (il > 0 && iw > 0 && *w != '*' && *w == *l) {
			w += 1;
			l += 1;
			il -= 1;
			iw -= 1;
		}
	} while (iw > 0 && *w == '*' &&  /* More to match a next wildcard? */
			(il > 0 || iw == 1));

	return iw == 0 && il == 0;
}

bool
dane_label_matches_label(ldns_rdf* w, ldns_rdf* l)
{
	uint8_t iw;
	uint8_t il;

	iw = ldns_rdf_data(w)[0];
	il = ldns_rdf_data(l)[0];
	return dane_wildcard_label_cmp(
			iw, (const char*)ldns_rdf_data(w) + 1,
			il, (const char*)ldns_rdf_data(l) + 1);
}

bool
dane_name_matches_server_name(const char* name_str, ldns_rdf* server_name)
{
	ldns_rdf* name;
	uint8_t nn, ns, i;
	ldns_rdf* ln;
	ldns_rdf* ls;
	
	name = ldns_dname_new_frm_str((const char*)name_str);
	if (! name) {
		LDNS_ERR(LDNS_STATUS_ERR, "ldns_dname_new_frm_str");
	}
	nn = ldns_dname_label_count(name);
	ns = ldns_dname_label_count(server_name);
	if (nn != ns) {
		ldns_rdf_free(name);
		return false;
	}
	ldns_dname2canonical(name);
	for (i = 0; i < nn; i++) {
		ln = ldns_dname_label(name, i);
		if (! ln) {
			return false;
		}
		ls = ldns_dname_label(server_name, i);
		if (! ls) {
			ldns_rdf_free(ln);
			return false;
		}
		if (! dane_label_matches_label(ln, ls)) {
			ldns_rdf_free(ln);
			ldns_rdf_free(ls);
			return false;
		}
		ldns_rdf_free(ln);
		ldns_rdf_free(ls);
	}
	return true;
}

bool
dane_X509_any_subject_alt_name_matches_server_name(
		X509 *cert, ldns_rdf* server_name)
{
	GENERAL_NAMES* names;
	GENERAL_NAME*  name;
	unsigned char* subject_alt_name_str = NULL;
	int i, n;

	names = X509_get_ext_d2i(cert, NID_subject_alt_name, 0, 0 );
	if (! names) { /* No subjectAltName extension */
		return false;
	}
	n = sk_GENERAL_NAME_num(names);
	for (i = 0; i < n; i++) {
		name = sk_GENERAL_NAME_value(names, i);
		if (name->type == GEN_DNS) {
			(void) ASN1_STRING_to_UTF8(&subject_alt_name_str,
					name->d.dNSName);
			if (subject_alt_name_str) {
				if (dane_name_matches_server_name((char*)
							subject_alt_name_str,
							server_name)) {
					OPENSSL_free(subject_alt_name_str);
					return true;
				}
				OPENSSL_free(subject_alt_name_str);
			}
		}
	}
	/* sk_GENERAL_NAMES_pop_free(names, sk_GENERAL_NAME_free); */
	return false;
}

bool
dane_X509_subject_name_matches_server_name(X509 *cert, ldns_rdf* server_name)
{
	X509_NAME* subject_name;
	int i;
	X509_NAME_ENTRY* entry;
	ASN1_STRING* entry_data;
	unsigned char* subject_name_str = NULL;
	bool r;
 
	subject_name = X509_get_subject_name(cert);
	if (! subject_name ) {
		ssl_err("could not X509_get_subject_name");
	}
	i = X509_NAME_get_index_by_NID(subject_name, NID_commonName, -1);
	entry = X509_NAME_get_entry(subject_name, i);
	entry_data = X509_NAME_ENTRY_get_data(entry);
	(void) ASN1_STRING_to_UTF8(&subject_name_str, entry_data);
	if (subject_name_str) {
		r = dane_name_matches_server_name(
				(char*)subject_name_str, server_name);
		OPENSSL_free(subject_name_str);
		return r;
	} else {
		return false;
	}
}

bool
dane_verify_server_name(X509* cert, ldns_rdf* server_name)
{
	ldns_rdf* server_name_lc;
	bool r;
	server_name_lc = ldns_rdf_clone(server_name);
	if (! server_name_lc) {
		LDNS_ERR(LDNS_STATUS_MEM_ERR, "ldns_rdf_clone");
	}
	ldns_dname2canonical(server_name_lc);
	r = dane_X509_any_subject_alt_name_matches_server_name(
			cert, server_name_lc) || 
	    dane_X509_subject_name_matches_server_name(
			cert, server_name_lc);
	ldns_rdf_free(server_name_lc);
	return r;
}

void
dane_create(ldns_rr_list* tlsas, ldns_rdf* tlsa_owner,
		ldns_tlsa_certificate_usage certificate_usage, int index,
		ldns_tlsa_selector          selector,
		ldns_tlsa_matching_type     matching_type,
		X509* cert, STACK_OF(X509)* extra_certs,
		X509_STORE* validate_store,
		bool verify_server_name, ldns_rdf* name)
{
	ldns_status s;
	X509* selected_cert;
	ldns_rr* tlsa_rr;

	if (verify_server_name && ! dane_verify_server_name(cert, name)) {
		fprintf(stderr, "The certificate does not match the "
				"server name\n");
		exit(EXIT_FAILURE);
	}

	s = ldns_dane_select_certificate(&selected_cert,
			cert, extra_certs, validate_store,
			certificate_usage, index);
	LDNS_ERR(s, "could not select certificate");

	s = ldns_dane_create_tlsa_rr(&tlsa_rr,
			certificate_usage, selector, matching_type,
			selected_cert);
	LDNS_ERR(s, "could not create tlsa rr");

	ldns_rr_set_owner(tlsa_rr, tlsa_owner);
			     
	if (! ldns_rr_list_contains_rr(tlsas, tlsa_rr)) {
		if (! ldns_rr_list_push_rr(tlsas, tlsa_rr)) {
			MEMERR("ldns_rr_list_push_rr");
		}
	}
}

bool
dane_verify(ldns_rr_list* tlsas, ldns_rdf* address,
		X509* cert, STACK_OF(X509)* extra_certs,
		X509_STORE* validate_store,
		bool verify_server_name, ldns_rdf* name)
{
	ldns_status s;
	char* address_str = NULL;

	s = ldns_dane_verify(tlsas, cert, extra_certs, validate_store);
	if (address) {
		address_str = ldns_rdf2str(address);
		fprintf(stdout, "%s", address_str ? address_str : "<address>");
		free(address_str);
	} else {
		X509_NAME_print_ex_fp(stdout,
				X509_get_subject_name(cert), 0, 0);
	}
	if (s == LDNS_STATUS_OK) {
		if (verify_server_name &&
				! dane_verify_server_name(cert, name)) {

			fprintf(stdout, " did not dane-validate, because:"
					" the certificate name did not match"
					" the server name\n");
			return false;
		}
		fprintf(stdout, " dane-validated successfully\n");
		return true;
	}
	fprintf(stdout, " did not dane-validate, because: %s\n",
			ldns_get_errorstr_by_id(s));
	return false;
}


int
main(int argc, char** argv)
{
	int c;
	enum { VERIFY, CREATE } mode = VERIFY;

	ldns_status   s;
	size_t        i;

	bool print_tlsa_as_type52   = false;
	bool assume_dnssec_validity = false;
	bool assume_pkix_validity   = false;
	bool verify_server_name     = true;

	char* CAfile    = NULL;
	char* CApath    = NULL;
	char* cert_file = NULL;
	X509* cert                  = NULL;
	STACK_OF(X509)* extra_certs = NULL;
	
	ldns_rr_list* keys = ldns_rr_list_new();
	size_t nkeys = 0;
	ldns_rr_list* dns_root = NULL;

	ldns_rr_list* addresses = ldns_rr_list_new();
	ldns_rr*      address_rr;
	ldns_rdf*     address;

	int           ai_family = AF_UNSPEC;
	int           transport = LDNS_DANE_TRANSPORT_TCP;

	char*         name_str;
	ldns_rdf*     name;
	uint16_t      port;

	ldns_resolver* res            = NULL;
	ldns_rdf*      tlsa_owner     = NULL;
	char*          tlsa_owner_str = NULL;
	ldns_rr_list*  tlsas          = NULL;

	ldns_rr_list*  originals      = NULL; /* original tlsas (before
					       * transform), but also used
					       * as temporary.
					       */

	ldns_tlsa_certificate_usage certificate_usage = 666;
	int                         index             =   0;
	ldns_tlsa_selector          selector          = 666;
	ldns_tlsa_matching_type     matching_type     = 666;
	

	X509_STORE *store  = NULL;

	SSL_CTX* ctx = NULL;
	SSL*     ssl = NULL;

	bool success = true;

	if (! keys || ! addresses) {
		MEMERR("ldns_rr_list_new");
	}
	while((c = getopt(argc, argv, "46a:bc:df:hi:k:np:r:su")) != -1) {
		switch(c) {
		case 'h':
			print_usage("ldns-dane");
			break;
		case '4':
			ai_family = AF_INET;
			break;
		case '6':
			ai_family = AF_INET6;
			break;
		case 'a':
			s = ldns_str2rdf_a(&address, optarg);
			if (s == LDNS_STATUS_OK) {
				address_rr = ldns_rr_new_frm_type(
						LDNS_RR_TYPE_A);
			} else {
				s = ldns_str2rdf_aaaa(&address, optarg);
				if (s == LDNS_STATUS_OK) {
					address_rr = ldns_rr_new_frm_type(
							LDNS_RR_TYPE_AAAA);
				} else {
					fprintf(stderr,
						"Could not interpret address "
						"%s\n",
						optarg);
					exit(EXIT_FAILURE);
				}
			}
			(void) ldns_rr_a_set_address(address_rr, address);
			for (i = 0; i < ldns_rr_list_rr_count(addresses); i++){
				if (ldns_rdf_compare(address,
				     ldns_rr_a_address(
				      ldns_rr_list_rr(addresses, i))) == 0) {
					break;
				}
			}
			if (i >= ldns_rr_list_rr_count(addresses)) {
				if (! ldns_rr_list_push_rr(addresses,
							address_rr)) {
					MEMERR("ldns_rr_list_push_rr");
				}
			}
			break;
		case 'b':
			print_tlsa_as_type52 = true;
			/* TODO: do it with output formats... maybe... */
			break;
		case 'c':
			cert_file = optarg; /* checking in SSL stuff below */
			break;
		case 'd':
			assume_dnssec_validity = true;
			break;
		case 'f':
			CAfile = optarg;
			break;
		case 'i':
			index = atoi(optarg); /* todo check if all numeric */
			break;
		case 'k':
			s = read_key_file(optarg, keys);
			LDNS_ERR(s, "Could not parse key file");
			if (ldns_rr_list_rr_count(keys) == nkeys) {
				fprintf(stderr, "No keys found in file %s\n",
						optarg);
				exit(EXIT_FAILURE);
			}
			nkeys = ldns_rr_list_rr_count(keys);
			break;
		case 'n':
			verify_server_name = false;
			break;
		case 'p':
			CApath = optarg;
			break;
		case 'r':
			dns_root = read_root_hints(optarg);
			if (!dns_root) {
				fprintf(stderr,
				"cannot read the root hints file\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 's':
			assume_pkix_validity = true;
			break;
		case 'u':
			transport = LDNS_DANE_TRANSPORT_UDP;
			break;
		}
	}

	/* Filter out given IPv4 addresses when -6 was given, 
	 * and IPv6 addresses when -4 was given.
	 */
	if (ldns_rr_list_rr_count(addresses) > 0 &&
			ai_family != AF_UNSPEC) {
		/* TODO: resource leak, previous addresses */
		originals = addresses;
		addresses = rr_list_filter_rr_type(originals,
				(ai_family == AF_INET
				 ? LDNS_RR_TYPE_A : LDNS_RR_TYPE_AAAA));
		ldns_rr_list_free(originals);
		if (addresses == NULL) {
			MEMERR("rr_list_filter_rr_type");
		}
		if (ldns_rr_list_rr_count(addresses) == 0) {
			fprintf(stderr,
				"No addresses of the specified type remain\n");
			exit(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 2) {
		print_usage("ldns-dane");
	}

	name_str = argv[0];
	s = ldns_str2rdf_dname(&name, name_str);
	LDNS_ERR(s, "could not ldns_str2rdf_dname");

	port = (uint16_t) usage_within_range(argv[1], 65535, "port");

	s = ldns_dane_create_tlsa_owner(&tlsa_owner, name, port, transport);
	LDNS_ERR(s, "could not create TLSA owner name");
	tlsa_owner_str = ldns_rdf2str(tlsa_owner);
	if (tlsa_owner_str == NULL) {
		MEMERR("ldns_rdf2str");
	}

	if (argc == 2) {

		mode = VERIFY;

		/* lookup tlsas */
		s = dane_setup_resolver(&res, keys, dns_root,
				assume_dnssec_validity);
		LDNS_ERR(s, "could not dane_setup_resolver");
		s = dane_query(&tlsas, res, tlsa_owner, LDNS_RR_TYPE_TLSA,
				LDNS_RR_CLASS_IN, false);
		ldns_resolver_free(res);

		if (s == LDNS_STATUS_DANE_INSECURE) {

			fprintf(stderr, "Warning! TLSA records for %s "
				"were found, but were insecure.\n"
				"PKIX validation without DANE will be "
				"performed. If you wish to perform DANE\n"
				"even though the RR's are insecure, "
				"se the -d option.\n", tlsa_owner_str);

		} else if (s != LDNS_STATUS_OK) {

			ldns_err("dane_query", s);

		} else if (ldns_rr_list_rr_count(tlsas) == 0) {

			fprintf(stderr, "Warning! No TLSA records for %s "
				"were found.\n"
				"PKIX validation without DANE will be "
				"performed.\n", ldns_rdf2str(tlsa_owner));

		} else if (assume_pkix_validity) { /* number of  tlsa's > 0 */
			
			/* transform type "CA constraint" to "Trust anchor
			 * assertion" and "Service Certificate Constraint"
			 * to "Domain Issues Certificate"
			 */
			originals = tlsas;
			tlsas = dane_no_pkix_transform(originals);
		}

	} else if (argc == 5) {

		mode = CREATE;

		tlsas = ldns_rr_list_new();

		certificate_usage = usage_within_range(argv[2], 3,
				"certificate usage");
		selector          = usage_within_range(argv[3], 1, "selector");
		matching_type     = usage_within_range(argv[4], 2,
				"matching type");

		if ((certificate_usage == LDNS_TLSA_USAGE_CA_CONSTRAINT ||
		     certificate_usage ==
			     LDNS_TLSA_USAGE_SERVICE_CERTIFICATE_CONSTRAINT) &&
		     ! CAfile && ! CApath && ! assume_pkix_validity) {

			fprintf(stderr,
				"When using the \"CA constraint\" or "
			        "\"Service certificate constraint\",\n"
				"-f <CAfile> and/or -p <CApath> options "
				"must be given to perform PKIX validation.\n\n"
				"PKIX validation may be turned off "
				"with the -s option. Note that with\n"
				"\"CA constraint\" the verification process "
				"should then end with a self-signed\n"
				"certificate which must be present "
				"in the server certificate chain.\n\n");

			exit(EXIT_FAILURE);
		}
	} else {
		print_usage("ldns-dane");
	}

	/* ssl inititalize */
	SSL_load_error_strings();
	SSL_library_init();

	/* ssl load validation store */
	if (! assume_pkix_validity || CAfile || CApath) {
		store = X509_STORE_new();
		if (! store) {
			ssl_err("could not X509_STORE_new");
		}
		if ((CAfile || CApath) && X509_STORE_load_locations(
					store, CAfile, CApath) != 1) {
			ssl_err("error loading CA certificates");
		}
	}

	ctx =  SSL_CTX_new(SSLv23_client_method());
	if (! ctx) {
		ssl_err("could not SSL_CTX_new");
	}
	if (cert_file &&
	    SSL_CTX_use_certificate_chain_file(ctx, cert_file) != 1) {
		ssl_err("error loading certificate");
	}
	ssl = SSL_new(ctx);
	if (! ssl) {
		ssl_err("could not SSL_new");
	}

	if (cert_file) { /* ssl load certificate */

		cert = SSL_get_certificate(ssl);
		if (! cert) {
			ssl_err("could not SSL_get_certificate");
		}
#ifndef S_SPLINT_S
		extra_certs = ctx->extra_certs;
#endif

		switch (mode) {
		case CREATE: dane_create(tlsas, tlsa_owner, certificate_usage,
					     index, selector, matching_type,
					     cert, extra_certs, store,
					     verify_server_name, name);
			     break;
		case VERIFY: if (! dane_verify(tlsas, NULL,
			                       cert, extra_certs, store,
					       verify_server_name, name)) {
				     success = false;
			     }
			     break;
		}

	} else {/* No certificate file given, creation/validation via TLS. */

		/* We need addresses to connect to */
		if (ldns_rr_list_rr_count(addresses) == 0) {
			s = dane_setup_resolver(&res, keys, dns_root,
					assume_dnssec_validity);
			LDNS_ERR(s, "could not dane_setup_resolver");
			ldns_rr_list_free(addresses);
			addresses =dane_lookup_addresses(res, name, ai_family);
			ldns_resolver_free(res);
		}
		if (ldns_rr_list_rr_count(addresses) == 0) {
			fprintf(stderr, "No addresses for %s\n", name_str);
			exit(EXIT_FAILURE);
		}

		/* for all addresses, setup SSL and retrieve certificates */
		for (i = 0; i < ldns_rr_list_rr_count(addresses); i++) {

			address = ldns_rr_a_address(
					ldns_rr_list_rr(addresses, i));
			assert(address != NULL);
			
			s = get_ssl_cert_chain(&cert, &extra_certs,
					ssl, address, port, transport);
			LDNS_ERR(s, "could not get cert chain from ssl");

			switch (mode) {

			case CREATE: dane_create(tlsas, tlsa_owner,
						     certificate_usage, index,
						     selector, matching_type,
						     cert, extra_certs, store,
						     verify_server_name, name);
				     break;

			case VERIFY: if (! dane_verify(tlsas, address,
						cert, extra_certs, store,
						verify_server_name, name)) {
					success = false;
				     }
				     break;
			}
		} /* end for all addresses */
	} /* end No certification file */

	if (mode == CREATE) {
		if (print_tlsa_as_type52) {
			print_rr_list_as_TYPEXXX(stdout, tlsas);
		} else {
			ldns_rr_list_print(stdout, tlsas);
		}
	}
	ldns_rr_list_deep_free(tlsas);

	/* cleanup */
	SSL_free(ssl);
	SSL_CTX_free(ctx);

	if (store) {
		X509_STORE_free(store);
	}
	if (tlsa_owner_str) {
		LDNS_FREE(tlsa_owner_str);
	}
	if (tlsa_owner) {
		ldns_rdf_free(tlsa_owner);
	}
	if (addresses) {
		ldns_rr_list_deep_free(addresses);
	}
	if (success) {
		exit(EXIT_SUCCESS);
	} else {
		exit(EXIT_FAILURE);
	}
}
#else

int
main(int argc, char **argv)
{
	fprintf(stderr, "ldns-dane needs OpenSSL support, "
			"which has not been compiled in\n");
	return 1;
}
#endif /* HAVE_SSL */
