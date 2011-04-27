/*
Copyright (c) 2011, Chris Double

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "polipo.h"
#include <jansson.h>
#include <curl/curl.h>

AtomPtr namecoindServer = NULL;
AtomPtr namecoindUsername = NULL;
AtomPtr namecoindPassword = NULL;
json_t* namecoind = NULL;


static int namecoindServerSetter(ConfigVariablePtr, void*);
static int namecoindUsernameSetter(ConfigVariablePtr, void*);
static int namecoindPasswordSetter(ConfigVariablePtr, void*);
static void scanNames();

void
preinitNamecoind()
{
    namecoindUsername = internAtom("");
    namecoindPassword = internAtom("");

    CONFIG_VARIABLE_SETTABLE(namecoindUsername, CONFIG_ATOM,
                             namecoindUsernameSetter,
                             "namecoind username");
    CONFIG_VARIABLE_SETTABLE(namecoindPassword, CONFIG_ATOM,
                             namecoindPasswordSetter,
                             "namecoind password");
    CONFIG_VARIABLE_SETTABLE(namecoindServer, CONFIG_ATOM_LOWER,
                             namecoindServerSetter,
                             "Server (host:port)");
    initNamecoind();
}

static int
namecoindServerSetter(ConfigVariablePtr var, void *value)
{
    configAtomSetter(var, value);
    initNamecoind();
    return 1;
}

static int
namecoindUsernameSetter(ConfigVariablePtr var, void *value)
{
    return configAtomSetter(var, value);
}

static int
namecoindPasswordSetter(ConfigVariablePtr var, void *value)
{
    return configAtomSetter(var, value);
}


void
initNamecoind()
{
    if(namecoindServer)
        scanNames();
}

#define BUFFER_SIZE  (256 * 1024)  /* 256 KB */

#define URL_FORMAT   "http://127.0.0.1:9332/"
#define URL_SIZE     256

/* Return the offset of the first newline in text or the length of
   text if there's no newline */
static int newline_offset(const char *text)
{
    const char *newline = strchr(text, '\n');
    if(!newline)
        return strlen(text);
    else
        return (int)(newline - text);
}

struct write_result
{
    char *data;
    int pos;
};

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream)
{
    struct write_result *result = (struct write_result *)stream;

    if(result->pos + size * nmemb >= BUFFER_SIZE - 1)
    {
        fprintf(stderr, "error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

static char *request(const char *url, const char* auth)
{
    CURL *curl;
    CURLcode status;
    char *data;
    long code;
    struct curl_slist* slist = NULL;

    curl = curl_easy_init();
    data = malloc(BUFFER_SIZE);
    if(!curl || !data)
        return NULL;

    struct write_result write_result = {
        .data = data,
        .pos = 0
    };

    slist = curl_slist_append(slist, "content-type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD, auth);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"version\": \"1.1\",\"method\": \"name_scan\",\"params\": [],\"id\": 1}");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen("{\"version\": \"1.1\",\"method\": \"name_scan\",\"params\": [],\"id\": 1}"));

    status = curl_easy_perform(curl);
    curl_slist_free_all(slist);
    if(status != 0)
    {
        fprintf(stderr, "error: unable to request data from %s:\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code != 200)
    {
        fprintf(stderr, "error: server responded with code %ld\n", code);
        return NULL;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;
}

char const* namecoind_lookup(char const* domain)
{
    json_error_t error;
    json_t* result;
    json_t* value;
    json_t* map;
    json_t* map_value;
    const char* suffix;
    int index;
    char* lookup;
    static time_t time = 0;

    if (current_time.tv_sec - time >= 600) {
        scanNames();
        time = current_time.tv_sec;
    }

    if(!namecoind)
        return NULL;

    suffix = strrchr(domain, '.');
    if (!suffix) 
        return NULL;
 
    if (strcmp(".bit", suffix) != 0) 
        return NULL;

    index = suffix - domain;
    lookup = strdup(domain);
    lookup[index] = '\0';
    result = json_object_get(namecoind, lookup);
    if(!result)
        return NULL;

    if(!json_is_string(result)) {
        fprintf(stderr, "error: namecoind_lookup is not a string\n");
        return NULL;
    }

    value = json_loads(json_string_value(result), 0, &error);
    if (!json_is_object(value)) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return NULL;
    }

    map = json_object_get(value, "map");
    if (!map || !json_is_object(map)) {
        fprintf(stderr, "error: namecoind name entry is not an object\n");
        return NULL;
    }

    // Look for IP address
    map_value = json_object_get(map, "");
    if (map_value && json_is_string(map_value)) {
        // Pointer is value until we do the next name refresh
        return json_string_value(map_value);
    }

    // Currently don't support other options
    return NULL;
}

static void
scanNames()
{
    json_error_t error;
    json_t* root;
    json_t* results;
    int i;
    char url[256];
    char auth[128];
    char* text;

    snprintf(url, sizeof(url), "http://%s/", namecoindServer->string);
    snprintf(auth, sizeof(auth), "%s:%s", namecoindUsername->string, namecoindPassword->string);
    text = request(url, auth); 

    if(!text)
        return;

    root = json_loads(text, 0, &error);
    free(text);

    if(!root)
    {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return;
    }

    results = json_object_get(root, "result");
    if(!json_is_array(results))
    {
        fprintf(stderr, "error: results is not an array\n");
        return;
    }

    if (namecoind) {
        json_decref(namecoind);
    }

    namecoind = json_object();
    if (!namecoind) {
        fprintf(stderr, "error: could not allocated namecoind object\n");
        return;
    }
    for(i = 0; i < json_array_size(results); i++)
    {
        json_t *result;
        json_t *name;
        json_t *value;
        result = json_array_get(results, i);
        if(!json_is_object(result))
        {
            fprintf(stderr, "error: result %d is not an object\n", i + 1);
            return;
        }

        name = json_object_get(result, "name");
        if(!json_is_string(name))
        {
            fprintf(stderr, "error: result %d: name is not a string\n", i + 1);
            return;
        }

        value = json_object_get(result, "value");
        if(!json_is_string(value))
        {
            fprintf(stderr, "error: result %d: value is not a string\n", i + 1);
            return;
        }

        const char* n = json_string_value(name);
        if(strlen(n) > 2 && n[0] == 'd' && n[1] == '/') 
            json_object_set(namecoind, n+2, value);
    }
}

