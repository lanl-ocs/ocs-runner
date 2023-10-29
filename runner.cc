/*
 * Copyright (c) 2021 Triad National Security, LLC, as operator of Los Alamos
 * National Laboratory with the U.S. Department of Energy/National Nuclear
 * Security Administration. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of TRIAD, Los Alamos National Laboratory, LANL, the
 *    U.S. Government, nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <spdk/env.h>
#include <spdk/nvme.h>
#include <spdk/nvmf_spec.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

namespace {

struct QueryState {
  QueryState() {}  // Intentionally not initialized for performance
  struct spdk_nvme_ctrlr* ctrlr;
  struct spdk_nvme_qpair* qp;
  size_t buf_size;
  char* buf;
  int total_result_size;
  int completed;
};

void OnResultsFetched(void* arg, const struct spdk_nvme_cpl* cpl) {
  QueryState* const s = static_cast<QueryState*>(arg);
  if (spdk_nvme_cpl_is_error(cpl)) {
    spdk_nvme_qpair_print_completion(s->qp,
                                     const_cast<struct spdk_nvme_cpl*>(cpl));
    fprintf(stderr, "I/O error status: %s\n",
            spdk_nvme_cpl_get_status_string(&cpl->status));
    abort();
  } else if (cpl->cdw1 != s->total_result_size) {
    fprintf(
        stderr,
        "The device said that total result is %d bytes but now it says it's %d "
        "bytes... not aborting but something must be wrong\n",
        s->total_result_size, cpl->cdw1);
  }
  fprintf(stderr, ">> query results obtained: %d B / %d B\n", cpl->cdw0,
          cpl->cdw1);
  s->buf[cpl->cdw0] = 0;
  printf("%s", s->buf);
  s->completed = 1;
}

void OnQueryPlanSubmitted(void* arg, const struct spdk_nvme_cpl* cpl) {
  QueryState* const s = static_cast<QueryState*>(arg);
  if (spdk_nvme_cpl_is_error(cpl)) {
    spdk_nvme_qpair_print_completion(s->qp,
                                     const_cast<struct spdk_nvme_cpl*>(cpl));
    fprintf(stderr, "I/O error status: %s\n",
            spdk_nvme_cpl_get_status_string(&cpl->status));
    abort();
  }
  fprintf(stderr, ">> query plan submitted: id=%d, result_size=%d B\n",
          cpl->cdw0, cpl->cdw1);
  s->total_result_size = cpl->cdw1;
  memset(s->buf, 0, s->buf_size);
  struct spdk_nvme_cmd cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.opc = 0x92;  // Get compute result
  cmd.nsid = 0x1;
  // Object space ID
  cmd.rsvd2 = 0;
  // Flags: type=s3 select | out=csv | head=no | release resource=yes
  cmd.rsvd3 = 0x020202;
  // Host buffer size
  cmd.cdw10 = s->buf_size;
  // Plan ID
  cmd.cdw11 = cpl->cdw0 << 16;
  // Seq_num | result offset
  cmd.cdw13 = 0;
  int rc = spdk_nvme_ctrlr_cmd_io_raw(s->ctrlr, s->qp, &cmd, s->buf,
                                      s->buf_size, OnResultsFetched, s);
  if (rc != 0) {
    fprintf(stderr,
            "Error performing spdk_nvme_ctrlr_cmd_io_raw() and i don't know "
            "how to recover... aborting now\n");
    abort();
  }
}

void RunOneQuery(struct spdk_nvme_ctrlr* ctrlr, struct spdk_nvme_ns* ns,
                 const std::string& query, int obj_id) {
  struct spdk_nvme_qpair* const qp =
      spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0);
  if (!qp) {
    fprintf(stderr,
            "Error performing spdk_nvme_ctrlr_alloc_io_qpair() and i don't "
            "know how to recover... aborting now\n");
    abort();
  }
  char* const buf = static_cast<char*>(spdk_zmalloc(
      0x1000, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA));
  if (!buf) {
    fprintf(stderr,
            "Error performing spdk_zmalloc() and i don't know how to "
            "recover... aborting now\n");
    abort();
  }
  strcpy(buf, query.c_str());
  struct spdk_nvme_cmd cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.opc = 0x95;  // Write execution plan
  cmd.nsid = 0x1;
  // Object space ID
  cmd.rsvd2 = 0;
  // Flags: type=s3 select | out=csv | run=yes | wait=yes
  cmd.rsvd3 = 0x020203;
  // Amount of query string transferred
  cmd.cdw10 = query.size();
  // Target object ID
  cmd.cdw11 = obj_id;
  // Plan ID
  cmd.cdw12 = 0xFFFF0000;
  // Plan offset
  cmd.cdw13 = 0;
  // Total query string size
  cmd.cdw14 = cmd.cdw10;
  QueryState s;
  s.completed = 0;
  s.ctrlr = ctrlr;
  s.qp = qp;
  s.buf_size = 0x1000;
  s.buf = buf;
  int rc = spdk_nvme_ctrlr_cmd_io_raw(ctrlr, qp, &cmd, buf, s.buf_size,
                                      OnQueryPlanSubmitted, &s);
  if (rc != 0) {
    fprintf(stderr,
            "Error performing spdk_nvme_ctrlr_cmd_io_raw() and i don't know "
            "how to recover... aborting now\n");
    abort();
  }
  while (!s.completed) {
    spdk_nvme_qpair_process_completions(qp, 0);
  }
  spdk_free(buf);
  spdk_nvme_ctrlr_free_io_qpair(qp);
}

}  // namespace

struct probe_cb_ctx {
  struct spdk_nvme_ctrlr* ctrlr;
  const char* subnqn;
};

bool probe_cb(void* cb_ctx, const struct spdk_nvme_transport_id* trid,
              struct spdk_nvme_ctrlr_opts* opts) {
  printf("Found %s+%s://%s:%s/%s\n",
         spdk_nvme_transport_id_trtype_str(trid->trtype),
         spdk_nvme_transport_id_adrfam_str(trid->adrfam), trid->traddr,
         trid->trsvcid, trid->subnqn);
  if (strcmp(static_cast<struct probe_cb_ctx*>(cb_ctx)->subnqn, trid->subnqn) ==
      0) {
    return true;
  } else {  // Skip it
    return false;
  }
}

void attach_cb(void* cb_ctx, const struct spdk_nvme_transport_id* trid,
               struct spdk_nvme_ctrlr* ctrlr,
               const struct spdk_nvme_ctrlr_opts* opts) {
  static_cast<struct probe_cb_ctx*>(cb_ctx)->ctrlr = ctrlr;
  printf("Attached to %s\n", trid->subnqn);
}

void probe_and_process(const struct spdk_nvme_transport_id* trid,
                       const char* subnqn) {
  struct probe_cb_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.subnqn = subnqn;
  int rc = spdk_nvme_probe(trid, &ctx, probe_cb, attach_cb, NULL);
  if (rc != 0) {
    fprintf(stderr, "spdk_nvme_probe() failed\n");
    return;
  }
  if (!ctx.ctrlr) {
    fprintf(stderr, "Error: cannot find %s\n", subnqn);
    return;
  }
  struct spdk_nvme_ns* ns = spdk_nvme_ctrlr_get_ns(ctx.ctrlr, 1);
  if (!ns || !spdk_nvme_ns_is_active(ns)) {
    fprintf(stderr, "Error: namespace 1 unavailable\n");
    return;
  }
  std::string query(
      "SELECT min(vertex_id) AS VID, min(x) as X, min(y) as Y, "
      "min(z) as Z, avg(e) AS E FROM s3object WHERE "
      "x > 1.5 AND x < 1.6 AND y > 1.5 AND y < 1.6 AND z > 1.5 AND z < 1.6 "
      "GROUP BY vertex_id ORDER BY E");
  RunOneQuery(ctx.ctrlr, ns, query, 0);
  spdk_nvme_detach(ctx.ctrlr);
  fprintf(stderr, "Done!\n");
}

void usage(char* argv0, const char* msg) {
  if (msg) fprintf(stderr, "%s: %s\n", argv0, msg);
  fprintf(stderr, "==============\n");
  fprintf(stderr, "Usage: sudo %s [options]\n\n", argv0);
  fprintf(stderr,
          "-t      trtype       :  Target transport type (e.g.: rdma)\n");
  fprintf(stderr, "-a      traddr       :  Target address (e.g.: 127.0.0.1)\n");
  fprintf(stderr, "-s      trsvcid      :  Service port (e.g.: 4420)\n");
  fprintf(stderr, "-n      subnqn       :  Name of subsystem\n");
  fprintf(stderr, "==============\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  int c;
  const char* trtype = "tcp";
  const char* adrfam = "ipv4";
  const char* traddr = "127.0.0.1";
  const char* subnqn = "nqn.2023-10.gov.lanl:xxx:ssd1";
  int trsvcid = 4420;
  while ((c = getopt(argc, argv, "a:n:s:t:h")) != -1) {
    switch (c) {
      case 'a':
        traddr = optarg;
        break;
      case 'n':
        subnqn = optarg;
        break;
      case 's':
        trsvcid = atoi(optarg);
        break;
      case 't':
        trtype = optarg;
        break;
      case 'h':
      default:
        usage(argv[0], NULL);
    }
  }
  struct spdk_env_opts opts;
  spdk_env_opts_init(&opts);
  opts.name = "runner";
  int rc = spdk_env_init(&opts);
  if (rc != 0) {
    fprintf(stderr, "spdk_env_init() failed\n");
    exit(EXIT_FAILURE);
  }
  char tmp[100];
  snprintf(tmp, sizeof(tmp),
           "trtype:%s adrfam:%s traddr:%s trsvcid:%d subnqn:%s", trtype, adrfam,
           traddr, trsvcid, SPDK_NVMF_DISCOVERY_NQN);
  struct spdk_nvme_transport_id trid;
  spdk_nvme_transport_id_parse(&trid, tmp);
  probe_and_process(&trid, subnqn);
  spdk_env_fini();
  return 0;
}
