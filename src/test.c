#include <stdlib.h>
#include <stdio.h>
#include "include/libhdcs.h"

struct hdcs_data {
  hdcs_ioctx_t io;
};

static int hdcs_connect(struct hdcs_data *hdcs, char* name)
{
	hdcs_open(&(hdcs->io), name);
	return 0;

}

static void hdcs_disconnect(struct hdcs_data *hdcs)
{
	if (!hdcs)
		return;

	/* shutdown everything */
  hdcs_close(hdcs->io);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    printf("usage:\n\t %s flush/promote ${hdcs_instance_name}\n\ttips: Make sure there is a general.conf under execution dir.\n\n", argv[0]);
    return 1;
  }
	struct hdcs_data *hdcs;
	hdcs = (struct hdcs_data*)calloc(1, sizeof(struct hdcs_data));
	if (!hdcs)
		goto failed;

  hdcs_connect(hdcs, argv[2]);
  if( argv[1][0] == 'p' ) {
    hdcs_promote_all(hdcs->io);
  } else {
    hdcs_flush_all(hdcs->io);
  }
  hdcs_disconnect(hdcs);
  return 0;

failed:
	if (hdcs)
		free(hdcs);
	return 1;
}
