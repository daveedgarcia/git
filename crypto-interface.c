/**
 *  File: crypto-interface.c
 *  Created by: Team Rogue (University of Portland)
 *                  - Vince Clasgens
 *                  - Dustin Dalen
 *                  - David Garcia
 *                  - Sam Chase
 *  Last Modified: April 2, 2013
 *
 *  Functions defined to sign and verify using 'git crypto'
 *  command using CMS library within OpenSSL
 *
 **/
#include "cache.h"
#include "commit.h"
#include "crypto-interface.h"
#include "diff.h"
#include "list-objects.h"
#include "log-tree.h"
#include "notes.h"
#include "object.h"
#include "revision.h"
#include "run-command.h"
#include "sigchain.h"
#include "strbuf.h"
#include "string-list.h"
#include <openssl/bio.h>
#include <openssl/cms.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/pkcs7.h>
#include <openssl/x509v3.h>
#include <openssl/x509.h>
#define BASH_ERROR -1

/**
 *  sha256()
 *
 *  Parameters: msg, outputBuffer
 *      -msg: input to be SHA'd
 *      -outputBuffer: buffer to be output (NEEDS to be 65 char's long)
 *
 *  Perform's SHA256 on a given input and returns it to the given buffer
 *
 **/
void sha256(char * msg, char outputBuffer[65])
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, msg, strlen(msg));
    SHA256_Final(hash, &sha256);
    int i = 0;
    for(i = 0; i < SHA256_DIGEST_LENGTH; ++i){
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;
}

/**
 *  create_bio()
 *
 *  Parameters: msg
 *      - msg: ref of object/note
 *
 *  Creates and allocates a BIO given the ref of the object or note passed in
 *
 **/
BIO * create_bio(char * msg)
{
    BIO * bio = BIO_new(BIO_s_mem());
    BIO_puts(bio, msg);
    return bio;
}

char ** get_commit_list()
{
    // Set up
    struct rev_info revs;
    init_revisions(&revs, NULL);
    revs.abbrev = DEFAULT_ABBREV;
    revs.commit_format = CMIT_FMT_UNSPECIFIED;

    const char *arg[] = {"rev-list", "--all"};
    setup_revisions(2, arg, &revs, NULL);
	if (prepare_revision_walk(&revs))
		die("revision walk setup failed");

    // Taken from traverse_commit_list
    int i;
    struct commit *commit;
    struct strbuf base;

    while((commit = get_revision(&revs)) != NULL){
        log_tree_commit(&revs, commit);
        if(commit->tree)
            add_pending_object(&revs, &(commit->tree->object), "");
    }

    char **list = malloc(sizeof(char*) * (revs.pending.nr + 1));
    for(i = 0; i < revs.pending.nr; ++i) {
		struct object_array_entry *pending = revs.pending.objects + i;
		struct object *obj = pending->item;
		const char *name = pending->name;
        list[i] = sha1_to_hex(obj->sha1);
    }
    list[revs.pending.nr] = NULL;

    return list;
}

void free_cmt_list(char ** list){
    if(list != NULL){
        free_cmt_list(&(list[1]));
        free(list);
    }
}

// Helper function which does "--ref=crypto"
void set_notes_ref(const char * ref)
{
    struct strbuf sb = STRBUF_INIT;
    strbuf_addstr(&sb, ref);
    expand_notes_ref(&sb);
    setenv("GIT_NOTES_REF", sb.buf, 1);
    strbuf_release(&sb);
}

/**
 *  get_note_from_commit()
 *
 *  Parameters: commit_ref
 *      - commit_ref: ref of a commit
 *
 *  Given ref of a given commit, searches for associated note in crypto
 *      notes namespace
 *      - if one is found the sha1 ref is returned
 *      - if none are found 0 is returned
 *
 **/
const unsigned char * get_note_for_commit(const char * commit_ref)
{
    struct notes_tree *t;
    unsigned char object[20];
    const unsigned char * note;

    // convert the hex to the commit object
    if(get_sha1(commit_ref, object))
        die(_("Failed to resolve '%s' as a valid ref."), commit_ref);

    // Set the ENV to the right namespace
    set_notes_ref("crypto");

    // Since the env is set &default_notes_tree points at crypto
    if(!default_notes_tree.initialized){
        init_notes(NULL, NULL, NULL, 0);
        t = &default_notes_tree;
    }

    // Get our note
    note = get_note(t, object);
    if(!note)
        return 0;
    return sha1_to_hex(note); // return the sha ref
}

/**
 *  get_object_from_sha1()

 *  Paramaters: ref
 *      - ref: SHA1 of a commit
 *
 *  Returns a pretty char* of the object
 *      - works with commits or notes
 *      - if no notes found, returns NULL
 *
 **/
char * get_object_from_sha1(const char * ref)
{
    unsigned char sha1[20];
    enum object_type type;
    unsigned long size;
    char * buf;
    //void * data; // Unused for now

    if(get_sha1(ref, sha1)){
        die(_("Failed to resolve '%s' as a valid ref."), ref);
    }

    // Get the type and size of the object(note)
    type = sha1_object_info(sha1, &size);
    // Get the pretty char *
    buf = read_sha1_file(sha1, &type, &size);

    // This line should return the blob but it's a function we
    // dont have access too easily
    //data = read_object(sha1, type, size);

    if(!buf)
        return NULL;
    return buf;
}

/**
 *  sign_commit()
 *
 *  Parameters: commit_sha
 *      - commit_sha: SHA1 of the ref of a commit
 *
 *  Signs the given commit SHA ref
 **/
int sign_commit(char *commit_sha){
    int ret_val = VERIFY_PASS; // TODO verify...?
    // Get the pretty commit
    char *commit = get_object_from_sha1(commit_sha);
    char commit_sha256[65];
    // Create the sha256
    sha256(commit, commit_sha256);

    // Get the tree to add the commit to
    struct notes_tree *t;
    set_notes_ref("crypto");
    init_notes(NULL, NULL, NULL, 0);
    t = &default_notes_tree;


    return ret_val;
}

// Move the signing method into crypto-interface.c

//look at crypto-interface.h for info
int sign_commit_sha256(EVP_KEY *key, X509* cert, X509_STORE* stack, char *cmt_sha)
{
    // get the pretty char* representation of the commit
    char *commit = get_object_from_sha1(commit_sha);
    // create the sha256 of it
    char commit_sha256[65];
    sha256(commit, commit_sha256);
    // create the bio we will use to sign
    BIO *input = create_bio(commit_sha256);

    //sign the message
    cms = CMS_sign(cert /*the certificate from .pem*/
                   ,key /*the private key from .pem*/
                   ,stack /*stack of x509 certs, unneeded*/
                   ,input /*the data to be signed, aka sha2 hash of commit*/
                   ,CMS_DETACHED); /* flag for cleartext signing */
    // TODO check for errors

    // TODO add notes

    /*
    BIO * in = NULL;
    X509 * cert = NULL;
    EVP_PKEY * key = NULL;
    int ret = 1;
    CMS_ContentInfo * cms = NULL;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    //get the path for our user certificate
    char * pem;
    get_pem_path(&pem);

    //trim the trailing whitespace
    char * end;
    end = pem + strlen(pem) - 1;
    while(end > pem && isspace(*end)) end--;
    //write new null terminator
    *(end+1) = 0;

    //read in .pem file
    in = BIO_new_file(pem,"r");

    //check for failure
    if(!in)
        goto err;

    //setting X509 * cert from BIO which read from pem
    cert = PEM_read_bio_X509(in, NULL, 0, NULL);

    BIO_reset(in);

    //read EVP_KEY * key from BIO which read from pem
    key = PEM_read_bio_PrivateKey(in, NULL, 0, NULL);

    //make sure these read in successfully
    if(!cert || !key)
        goto err;

    //char array to hold the sha2 hash
    char calc_hash[65];

    //get the human readable actual commit
    char * commit_head = get_object_from_sha1(sha);

    //SHA2 on the char* that contains the commit path
    sha256(commit_head, calc_hash);

    //put the hash into a BIO *
    BIO * data = BIO_new(BIO_s_mem());
    BIO_puts(data, calc_hash);

    //check for failure
    if(!data)
        goto err;


    //check for failure
    if(!cms)
        goto err;

    ret = 0;

err:
    if(ret)
    {
        fprintf(stderr, "Error Signing Data\n");
        ERR_print_errors_fp(stderr);
    }

    if(cert)
        X509_free(cert);
    if(key)
        EVP_PKEY_free(key);
    if(data)
        BIO_free(data);
    if(in)
        BIO_free(in);

    return ret;
    */
}

//look at crypto-interface.h for info
int verify_commit(char *commit_sha)
{
    int ret_val = VERIFY_PASS;

    // Get the note for the commit
    set_notes_ref("crypto");
    const unsigned char *note_sha = get_note_for_commit(commit_sha);
    if(!note_sha){ // If no note we dont have anything to do
        return VERIFY_FAIL_NO_NOTE;
    }
    // Get our commit and s/mime note
    char *note = get_object_from_sha1(note_sha);
    char *commit = get_object_from_sha1(commit_sha);

    // OpenSSL inst vars
    BIO *note_bio = NULL;
    BIO *content = NULL;
    CMS_ContentInfo *cms = NULL;
    X509_STORE *x509_st = X509_STORE_new(); // should be a param

    // Construct the objects needed to verify
    note_bio = create_bio(note);
    cms = SMIME_read_CMS(note_bio, &content);

    // Verify the s/smime message
    int err = CMS_verify(cms
                       , NULL /*stack x509*/
                       , x509_st
                       , note_bio
                       , NULL /*out bio not used*/
                       , CMS_NO_SIGNER_CERT_VERIFY);
    if(err){ // if an error we need to parse it TODO
        ret_val = ret_val | VERIFY_FAIL_BAD_SIG;
    }

    // Get the sha256 of our commit object to comparet
    static unsigned char commit_sha2[65];
    sha256(commit, commit_sha2);

    // TODO know the file format perfectly of the smime
    // to extract the original sha256
    char * buf = malloc(sizeof(char) * 65);
    BIO_read(content, buf, 65);
    printf("cms: %s\n", buf);
    // We need to strcmp the two, aka no big deal

    return ret_val;
}

