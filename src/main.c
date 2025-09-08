#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>

#include <sysrepo.h>
#include <libyang/libyang.h>

static volatile sig_atomic_t g_stop = 0;

int set_schema_mount(sr_session_ctx_t *sess) {
  const char *inline_path = \
    "/ietf-yang-schema-mount:schema-mounts/mount-point[module='acc-host'][label='test']/inline";
  if (sr_set_item_str(sess, inline_path, NULL, NULL, SR_EDIT_DEFAULT) != SR_ERR_OK) {
    fprintf(stderr, "Failed to create inline mount node at %s\n", inline_path);
    return 1;
  }
  if (sr_apply_changes(sess, 0) != SR_ERR_OK) {
    fprintf(stderr, "Failed to apply changes schema mount\n");
    return 1;
  }
  return 0;
}

int get_sm_yanglib_data(struct lyd_node **sm_yanglib, struct ly_ctx **out_ctx) {
  struct ly_ctx *ctx = NULL;
  if (ly_ctx_new(NULL, 0, &ctx) != LY_SUCCESS) {
    fprintf(stderr, "Failed to create ly context\n");
    return 1;
  }
  fprintf(stderr, "Created ly context\n");

  char search_dir_buffer[1024];
  const char *sr_repo_path = sr_get_repo_path();
  if (sprintf(&search_dir_buffer[0], "%s/yang", sr_repo_path) <= 0) {
    fprintf(stderr, "Failed to concatenate search dir parts\n");
    ly_ctx_destroy(ctx);
    return 1;
  }
  char *search_dir = &search_dir_buffer[0];
  fprintf(stderr, "Got search dir %s\n", search_dir);
  if (ly_ctx_set_searchdir(ctx, search_dir) != LY_SUCCESS) {
    fprintf(stderr, "Failed to set search dir %s\n", search_dir);
    ly_ctx_destroy(ctx);
    return 1;
  }
  fprintf(stderr, "Set search dir %s\n", search_dir);

  const char* const* search_dirs = ly_ctx_get_searchdirs(ctx);
  for (int i = 0; search_dirs[i] != NULL; i++) {
    fprintf(stderr, "Search dir %d: %s\n", i, search_dirs[i]);
  }

  const char *module_name = "acc-foo";
  struct lys_module *mod = ly_ctx_load_module(ctx, module_name, NULL, NULL);
  if (mod == NULL) {
    fprintf(stderr, "Failed to load acc-foo module\n");
    ly_ctx_destroy(ctx);
    return 1;
  }
  fprintf(stderr, "Loaded acc-foo module\n");

  if (ly_ctx_get_yanglib_data(ctx, sm_yanglib, "%u", ly_ctx_get_change_count(ctx)) != LY_SUCCESS) {
    fprintf(stderr, "Failed to get yanglib data\n");
    ly_ctx_destroy(ctx);
    return 1;
  }
  fprintf(stderr, "Got yanglib data\n");
  *out_ctx = ctx;
  return 0;
}

int set_yang_library(sr_session_ctx_t *sess) {
  struct lyd_node *sm_yanglib = NULL;
  struct ly_ctx *sm_ctx = NULL;
  if (get_sm_yanglib_data(&sm_yanglib, &sm_ctx)) {
    fprintf(stderr, "Failed to get sm yanglib data\n");
    return 1;
  }
  if (sm_yanglib == NULL) {
    fprintf(stderr, "Sm yanglib data is NULL\n");
    if (sm_ctx) ly_ctx_destroy(sm_ctx);
    return 1;
  }
  fprintf(stderr, "Got sm yanglib data\n");
  
  struct ly_ctx* sess_ly_ctx = (struct ly_ctx*)sr_session_acquire_context(sess);
  sr_release_context(sr_session_get_connection(sess));
  
  struct lyd_node *full_path_sm_yanglib = NULL;
  if (lyd_new_path(NULL, sess_ly_ctx, "/acc-host:test", NULL, 0, &full_path_sm_yanglib) != LY_SUCCESS) {
    fprintf(stderr, "Failed to create full path sm yanglib\n");
    return 1;
  }
  fprintf(stderr, "Created full path sm yanglib\n");
  
  // duplicate yanglib subtree into the session context under the mount-point
  struct lyd_node *duplicated_first = NULL;
  if (lyd_dup_siblings_to_ctx(sm_yanglib, sess_ly_ctx, (struct lyd_node_inner*)full_path_sm_yanglib, LYD_DUP_RECURSIVE, &duplicated_first) != LY_SUCCESS) {
    fprintf(stderr, "Failed to duplicate yanglib subtree under mount-point (cross-context)\n");
    lyd_free_all(full_path_sm_yanglib);
    lyd_free_all(sm_yanglib);
    ly_ctx_destroy(sm_ctx);
    return 1;
  }
  fprintf(stderr, "Duplicated yanglib subtree under mount-point\n");

  // free temporary context/tree
  lyd_free_all(sm_yanglib);
  ly_ctx_destroy(sm_ctx);

  char *operation = "merge";
  if (sr_edit_batch(sess, full_path_sm_yanglib, operation) != SR_ERR_OK) {
    fprintf(stderr, "Failed to edit batch yang library\n");
    lyd_free_all(full_path_sm_yanglib);
    return 1;
  }
  fprintf(stderr, "Edited batch yang library\n");

  if (sr_apply_changes(sess, 0) != SR_ERR_OK) {
    fprintf(stderr, "Failed to apply changes yang library\n");
    lyd_free_all(full_path_sm_yanglib);
    return 1;
  }
  fprintf(stderr, "Applied changes yang library\n");

  lyd_free_all(full_path_sm_yanglib);
  return 0;
}

int set_foo_data(sr_session_ctx_t *sess) {
  const char *path = "/acc-host:test/acc-foo:foo/bar";
  if (sr_set_item_str(sess, path, "baz", NULL, SR_EDIT_DEFAULT) != SR_ERR_OK) {
    fprintf(stderr, "Failed to set foo data\n");

    const sr_error_info_t *error_info = NULL;
    if (sr_session_get_error(sess, &error_info) != SR_ERR_OK) {
      fprintf(stderr, "Failed to get error info\n");
    } else {
      fprintf(stderr, "Got error info\n");
      for (int i = 0; i < error_info->err_count; i++) {
        sr_error_info_err_t err = error_info->err[i];
        fprintf(stderr, "Error %d: %d, %s, %s, %p\n", i, err.err_code, err.message, err.error_format, err.error_data);
      }
    }

    return 1;
  }
  fprintf(stderr, "Set foo data\n");

  if (sr_apply_changes(sess, 0) != SR_ERR_OK) {
    fprintf(stderr, "Failed to apply changes foo data\n");
    return 1;
  }
  fprintf(stderr, "Applied changes foo data\n");
  return 0;
}

int ncsm_run() {
  sr_conn_ctx_t *conn = NULL;
  if (sr_connect(0, &conn) != SR_ERR_OK) {
    fprintf(stderr, "Failed to connect to sysrepo\n");
    return 1;
  }
  fprintf(stderr, "Connected to sysrepo\n");

  sr_session_ctx_t *sess = NULL;
  if (sr_session_start(conn, SR_DS_OPERATIONAL, &sess) != SR_ERR_OK) {
    fprintf(stderr, "Failed to start session\n");
    return 1;
  }
  fprintf(stderr, "Started session\n");

  if (set_schema_mount(sess)) {
    fprintf(stderr, "Failed to set schema mount\n");
    return 1;
  }
  fprintf(stderr, "Set schema mount\n");

 if (set_yang_library(sess)) {
    fprintf(stderr, "Failed to set yang library\n");
    return 1;
  }
  fprintf(stderr, "Set yang library\n");

  if (set_foo_data(sess)) {
    fprintf(stderr, "Failed to set foo data\n");
    return 1;
  }
  fprintf(stderr, "Set foo data\n");

  // keep running to connect to the netopeer2-server
  sleep(10000);

  sr_session_stop(sess);
  fprintf(stderr, "Stopped session\n");

  sr_disconnect(conn);
  fprintf(stderr, "Disconnected from sysrepo\n");

  return 0;
}

int main(int argc, char **argv) {
  return ncsm_run();
}
