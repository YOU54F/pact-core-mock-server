#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <curl/curl.h>
#include <pact.h>

char *append_filename(char *executable, char *filename) {
  int executable_len = strlen(executable);
  int filename_len = strlen(filename);
  int package_len = strlen(PACKAGE_NAME);
  int diff = executable_len - package_len;
  char *string = malloc(diff + filename_len + 1);
  memcpy(string, executable, diff);
  memcpy(string + diff, filename, filename_len);
  string[diff + filename_len] = 0;
  return string;
}

char *slurp_file(char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (fp) {
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *string = malloc(fsize + 1);
    int read = fread(string, fsize, 1, fp);
    string[fsize] = 0;
    fclose(fp);
    return string;
  } else {
    printf("Failed to read %s\n", filename);
    return 0;
  }
}

/*
  Definitions of the exported functions from the pact mock server library
*/
typedef int32_t (*lib_create_mock_server)(const char *, const char*);
typedef int32_t (*lib_mock_server_matched)(int32_t);
typedef int32_t (*lib_cleanup_mock_server)(int32_t);
typedef char* (*lib_mock_server_mismatches)(int32_t);

typedef int (*lib_log_to_buffer)(enum LevelFilter level_filter);
typedef int (*lib_log_to_stdout)(enum LevelFilter level_filter);
typedef const char * (*lib_fetch_log_buffer)(const char *);
typedef const char * (*lib_mock_server_logs)(int32_t);
typedef void (*lib_string_delete)(const char *string);

lib_create_mock_server create_mock_server;
lib_mock_server_matched mock_server_matched;
lib_cleanup_mock_server cleanup_mock_server;
lib_mock_server_mismatches mock_server_mismatches;

lib_log_to_buffer mock_server__log_to_buffer;
lib_log_to_stdout mock_server__log_to_stdout;
lib_fetch_log_buffer mock_server__fetch_log_buffer;
lib_mock_server_logs mock_server__mock_server_logs;
lib_string_delete mock_server__string_delete;

/* Loads the mock server shared library and sets up the functions we need to call */
int setup_mock_server_functions(char *mock_server_lib) {
  /* Get a handle to the pact mock server library*/
  void *handle = dlopen(mock_server_lib, RTLD_NOW | RTLD_GLOBAL);
  if (handle) {
    /* We have a handle, so lookup the functions we need */
    create_mock_server = dlsym(handle, "pactffi_create_mock_server");
    mock_server_matched = dlsym(handle, "pactffi_mock_server_matched");
    cleanup_mock_server = dlsym(handle, "pactffi_cleanup_mock_server");
    mock_server_mismatches = dlsym(handle, "pactffi_mock_server_mismatches");
    mock_server__log_to_buffer = dlsym(handle, "pactffi_log_to_buffer");
    mock_server__log_to_stdout = dlsym(handle, "pactffi_log_to_stdout");
    mock_server__fetch_log_buffer = dlsym(handle, "pactffi_fetch_log_buffer");
    mock_server__mock_server_logs = dlsym(handle, "pactffi_mock_server_logs");
    mock_server__string_delete = dlsym(handle, "pactffi_string_delete");
    return create_mock_server != 0 && mock_server_matched != 0 && cleanup_mock_server != 0 &&
      mock_server_mismatches != 0;
  } else {
    printf("Failed to open shared library %s\n", dlerror());
    return 0;
  }
}

/* Execute the basic test against the provider server */
void execute_basic_test(int port) {
  CURL *curl = curl_easy_init();
  if (curl) {
    char url[64];
    sprintf(url, "http://localhost:%d/mallory?name=ron&status=good", port);
    printf("Executing request against %s\n", url);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      puts("\nRequest failed");
    }
    puts("\n");
    curl_easy_cleanup(curl);
  } else {
    puts("Could not initialise the curl library.");
  }
}

/*
  Run a basic test sing the simple_pact.json file
*/
void basic_test(char *executable) {
  /* Load the pact file into memory */
  char *pactfile = append_filename(executable, "simple_pact.json");
  char *pact = slurp_file(pactfile);
  if (pact) {
    /* Create the mock server from the pact file. The mock server port will be returned */
    int port = create_mock_server(pact, "127.0.0.1:0");
    if (port > 0) {
      printf("Mock server started on port %d\n", port);

      /* Now we execute out test against the mock server */
      execute_basic_test(port);

      /* Check the result */
      if (mock_server_matched(port)) {
        puts("OK: Mock server verified all requests, as expected");
      } else {
        puts("FAILED: Mock server did not match all requests!!");
      }

      puts("--------------- MOCK SERVER LOGS ---------------");
      const char* logs = mock_server__mock_server_logs(port);
      puts(logs);
      puts("------------------------------------------------");

      /* Lastly, we need to shutdown and cleanup the mock server */
      cleanup_mock_server(port);

      free(pact);
    } else {
      printf("Failed to start mock_server %d\n", port);
    }
  } else {
    printf("Failed to read %s\n", pactfile);
  }
  free(pactfile);
}

char *error_data = "{\"complete\": {\"body\":123457}, \"body\": [1,2,3]}\n";

size_t error_test_read_callback(char *buffer, size_t size, size_t nitems, void *instream) {
  strcpy(buffer, error_data);
  return strlen(error_data);
}

/* Execute the error test against the provider server, where we expect validations to fail */
void execute_error_test(int port) {
  CURL *curl = curl_easy_init();
  if (curl) {
    char url[32];
    sprintf(url, "http://localhost:%d/?test=hi", port);
    printf("Executing request against %s\n", url);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, strlen(error_data));

    struct curl_slist *list = NULL;
    list = curl_slist_append(list, "Content-Type: application/json");
    list = curl_slist_append(list, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, error_test_read_callback);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      printf("\nRequest failed: %d - %s\n", res,  curl_easy_strerror(res));
    }
    puts("\n");
    curl_easy_cleanup(curl);
  } else {
    puts("Could not initialise the curl library.");
  }
}

/*
  Run a error test sing the test_pact_with_bodies.json file. This test is expected to have some verification errors.
*/
void error_test(char *executable) {
  /* Load the pact file into memory */
  char *pactfile = append_filename(executable, "test_pact_with_bodies.json");
  char *pact = slurp_file(pactfile);
  if (pact) {
    /* Create the mock server from the pact file. The mock server port will be returned */
    int port = create_mock_server(pact, "127.0.0.1:0");
    if (port > 0) {
      printf("Mock server started on port %d\n", port);

      /* Now we execute out test against the mock server */
      execute_error_test(port);

      /* Check the result */
      if (mock_server_matched(port)) {
        puts("FAILED: Mock server verified all requests!!");
      } else {
        puts("OK: Mock server did not match all requests.");
        char *mismatch_json = mock_server_mismatches(port);
        puts(mismatch_json);
      }

      puts("--------------- MOCK SERVER LOGS ---------------");
      const char* logs = mock_server__mock_server_logs(port);
      puts(logs);
      puts("------------------------------------------------");

      /* Lastly, we need to shutdown and cleanup the mock server */
      cleanup_mock_server(port);
      free(pact);
    } else {
      printf("Failed to start mock_server %d\n", port);
    }
  } else {
    printf("Failed to read %s\n", pactfile);
  }
  free(pactfile);
}

int main (int argc, char **argv) {
  puts("This is " PACKAGE_STRING ".");

  if (argc < 3 || (strcmp(argv[1], "basic") != 0 && strcmp(argv[1], "error") != 0 && strcmp(argv[1], "both") != 0)) {
    puts("You need to specify the test to run: basic, error or both and the path to the pact FFI DLL");
    return 1;
  }

  if (!setup_mock_server_functions(argv[2])) {
    puts("Failed to setup the mock server library functions, exiting.\n");
    return 1;
  }

  int result = mock_server__log_to_buffer(LevelFilter_Debug);
  printf("Setup logging result: %d\n", result);

  curl_global_init(CURL_GLOBAL_ALL);

  if (strcmp(argv[1], "basic") == 0) {
    puts("Running basic pact test");
    basic_test(argv[0]);
  } else if (strcmp(argv[1], "error") == 0) {
    puts("Running error pact test");
    error_test(argv[0]);
  } else if (strcmp(argv[1], "both") == 0) {
    puts("Running both tests test");
    basic_test(argv[0]);
    error_test(argv[0]);
  } else {
    puts("Hmm, I'm sure I validated all the inputs, so how did you get here?");
  }

  puts("------------------ GLOBAL LOGS ------------------");
  const char* logs = mock_server__fetch_log_buffer(NULL);
  puts(logs);
  mock_server__string_delete(logs);
  puts("------------------------------------------------");

  return 0;
}
