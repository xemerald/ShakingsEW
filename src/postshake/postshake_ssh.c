#include <libssh/libssh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define DEFAULT_SSH_PORT 22

/* Prototype of function */
static int verify_knownhost(ssh_session);


ssh_session connect_ssh ( const char *host, long port, const char *user, const char *password )
{
	int rc, count = 0;
	ssh_session my_session;

	my_session = ssh_new();

	if ( my_session == NULL ) return NULL;

	if ( ssh_options_set(my_session, SSH_OPTIONS_HOST, host) < 0 )
	{
		printf( "postshake: Set SSH session host failed!\n" );
		ssh_disconnect(my_session);
		ssh_free(my_session);
		return NULL;
	}
	
	if ( port != DEFAULT_SSH_PORT )
	{	
		if ( ssh_options_set(my_session, SSH_OPTIONS_PORT, &port) < 0 )
		{
			printf( "postshake: Set SSH session port failed!\n" );
			ssh_disconnect(my_session);
			ssh_free(my_session);
			return NULL;
		}
	}

	if ( ssh_options_set(my_session, SSH_OPTIONS_USER, user) < 0 )
	{
		printf( "postshake: Set SSH session user failed!\n" );
		ssh_disconnect(my_session);
		ssh_free(my_session);
		return NULL;
	}

	if ( ssh_connect(my_session) != SSH_OK )
	{
		printf( "postshake: SSH connection failed!\n" );
		ssh_disconnect(my_session);
		ssh_free(my_session);
		return NULL;
	}

	if ( verify_knownhost(my_session) < 0 )
	{
		ssh_disconnect(my_session);
		ssh_free(my_session);
		return NULL;
	}


	while( count++ < 5 )
	{
		if ( (rc = ssh_userauth_password(my_session, NULL, password)) == SSH_AUTH_SUCCESS )
		{
			printf( "postshake: SSH authentication success!\n" );
			return my_session;
		}
		else if ( rc == SSH_AUTH_AGAIN )
		{
			sleep(1);
			printf( "postshake: No response! Try SSH authentication again...\n" );
			continue;
		}
		else break;
	}

	printf( "postshake: SSH authenticating error: %s\n", ssh_get_error(my_session) );
	ssh_disconnect(my_session);
	ssh_free(my_session);
	return NULL;
}


int verify_knownhost ( ssh_session session )
{
	int state;
	size_t hlen;
	unsigned char *hash = NULL;
	char *hexa;
	ssh_key key;


	state = ssh_is_server_known(session);

	ssh_get_publickey(session, &key);
	ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_MD5, &hash, &hlen);

	if ( hash == NULL ) return -1;

	switch (state)
	{
		case SSH_SERVER_KNOWN_OK:
			break; /* ok */
		case SSH_SERVER_KNOWN_CHANGED:
			printf( "postshake: SSH host key for server changed: it is now:\n" );
			ssh_print_hexa("postshake: Public key hash", hash, hlen);
			printf( "postshake: For security reasons, SSH connection will be stopped\n" );
			free(hash);
			return -1;
		case SSH_SERVER_FOUND_OTHER:
			printf( "postshake: The host key for this server was not found but an other"
					"type of key exists.\n" );
			printf( "postshake: An attacker might change the default server key to"
					"confuse your client into thinking the key does not exist\n" );
			free(hash);
			return -1;
		case SSH_SERVER_FILE_NOT_FOUND:
			printf( "postshake: Could not find known host file.\n" );
			printf( "postshake: If you accept the host key here, the file will be"
					"automatically created.\n" );
	/* fallback to SSH_SERVER_NOT_KNOWN behavior */
		case SSH_SERVER_NOT_KNOWN:
			hexa = ssh_get_hexa(hash, hlen);
			printf( "postshake: The server is unknown. We are going to add the host key.\n" );
			printf( "postshake: Public key hash: %s\n", hexa );
			free(hexa);
			if (ssh_write_knownhost(session) < 0)
			{
				printf( "postshake: Error %s\n", strerror(errno) );
				free(hash);
				return -1;
			}
			break;
		case SSH_SERVER_ERROR:
			printf( "postshake: Error %s", ssh_get_error(session) );
			free(hash);
			return -1;
	}

	free(hash);
	return 0;
}


int do_scp( ssh_session session, const char *destination, const char *filepath )
{
	char buffer[16384];
	int r, w;
	int size;
	int mode;
	int total = 0;

	char *filename;
	FILE *srcfile = NULL;

	socket_t fd; 
	struct stat s; 

	ssh_scp scp = ssh_scp_new( session, SSH_SCP_WRITE, destination );


	if ( ssh_scp_init(scp) != SSH_OK )
	{
		printf( "postshake: Error initializing scp: %s\n",ssh_get_error(session) );
		return -1;
	}

	srcfile = fopen( filepath, "rb" );
	fd = fileno(srcfile);
	fstat(fd, &s);
	size = s.st_size;
	mode = s.st_mode & ~S_IFMT;
	filename = ssh_basename(filepath);

	r = ssh_scp_push_file( scp, filename, size, mode );
	if ( r == SSH_ERROR )
	{
		printf( "postshake: Error pushing scp file: %s\n",ssh_get_error(session) );
		ssh_scp_free(scp);
		return -1;
	}
	
	do
	{
		r = fread( buffer, 1, sizeof(buffer), srcfile );

		if ( r == 0 ) break;
		if ( r < 0 )
		{
			printf( "postshake: Error reading file: %s\n",strerror(errno) );
			return -1;
		}

		w = ssh_scp_write( scp, buffer, r );
		if ( w == SSH_ERROR )
		{
			printf( "postshake: Error writing in scp: %s\n",ssh_get_error(session) );
			ssh_scp_free(scp);
			return -1;
		}

		total += r;

	} while( total < size );

	printf( "postshake: Wrote %d bytes to FTP server!\n", total );
	
	r = ssh_scp_close(scp); 
	if ( r == SSH_ERROR )
	{
		printf( "postshake: Error closing scp: %s\n",ssh_get_error(session) );
		ssh_scp_free(scp);
		return -1;
	} 
	ssh_scp_free(scp);

	fclose(srcfile); 
	srcfile = NULL; 
	free(filename);

	return 0;
}
