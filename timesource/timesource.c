#include <glib.h>
#include <glib-unix.h>
#include <gst/gst.h>
#include <gst/net/gstnet.h>
#include <stdio.h>


static gboolean exit_sighandler(gpointer user_data)
{
	GMainLoop *mainloop = (GMainLoop*)(user_data);

	fprintf(stderr, "caught signal, stopping mainloop\n");
	g_main_loop_quit(mainloop);

	return TRUE;
}


int main(int argc, char *argv[])
{
	GMainLoop *mainloop;
	GstClock *system_clock;
	GstNetTimeProvider *nettimeprovider;
	guint port;

	gst_init(&argc, &argv);

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <nettimeprovider port number>\n", argv[0]);
		return -1;
	}

	port = g_ascii_strtoull(argv[1], NULL, 10);
	if (port < 1 || port > 65535)
	{
		fprintf(stderr, "Invalid port %u - must be in the 1-65535 range\n", port);
		return -1;
	}

	mainloop = g_main_loop_new(NULL, FALSE);

	system_clock = gst_system_clock_obtain();
	nettimeprovider = gst_net_time_provider_new(system_clock, NULL, port);
	gst_object_unref(GST_OBJECT(system_clock));

	g_unix_signal_add(SIGINT, exit_sighandler, mainloop);
	g_unix_signal_add(SIGTERM, exit_sighandler, mainloop);

	g_main_loop_run(mainloop);

	g_main_loop_unref(mainloop);
	gst_object_unref(GST_OBJECT(nettimeprovider));

	return 0;
}
