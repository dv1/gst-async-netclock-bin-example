#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include "gst_async_netclock_bin.h"


GST_DEBUG_CATEGORY_STATIC(async_netclock_bin_debug);
#define GST_CAT_DEFAULT async_netclock_bin_debug


enum
{
	PROP_0,
	PROP_TIME_SOURCE_ADDRESS,
	PROP_TIME_SOURCE_PORT
};


#define DEFAULT_TIME_SOURCE_ADDRESS NULL
#define DEFAULT_TIME_SOURCE_PORT 6161


static GstStaticPadTemplate static_src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(GST_AUDIO_CAPS_MAKE(GST_AUDIO_FORMATS_ALL))
);


#define gst_async_netclock_bin_parent_class parent_class
G_DEFINE_TYPE(GstAsyncNetclockBin, gst_async_netclock_bin, GST_TYPE_BIN);


static void gst_async_netclock_bin_dispose(GObject *object);
static void gst_async_netclock_bin_set_property(GObject *object, guint prop_id, GValue const *value, GParamSpec *pspec);
static void gst_async_netclock_bin_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static GstStateChangeReturn gst_async_netclock_bin_change_state(GstElement *element, GstStateChange transition);
static GstClock* gst_async_netclock_bin_provide_clock(GstElement *element);

static void gst_async_netclock_bin_on_clock_synced(GstClock *clock, gboolean synced, gpointer user_data);




static void gst_async_netclock_bin_class_init(GstAsyncNetclockBinClass *klass)
{
	GObjectClass *object_class;
	GstElementClass *element_class;

	GST_DEBUG_CATEGORY_INIT(async_netclock_bin_debug, "asyncnetclockbin", 0, "example source bin with asynchronous netclientclock synchronization");

	object_class = G_OBJECT_CLASS(klass);
	element_class = GST_ELEMENT_CLASS(klass);

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&static_src_template));

	object_class->dispose        = GST_DEBUG_FUNCPTR(gst_async_netclock_bin_dispose);
	object_class->set_property   = GST_DEBUG_FUNCPTR(gst_async_netclock_bin_set_property);
	object_class->get_property   = GST_DEBUG_FUNCPTR(gst_async_netclock_bin_get_property);
	element_class->change_state  = GST_DEBUG_FUNCPTR(gst_async_netclock_bin_change_state);
	element_class->provide_clock = GST_DEBUG_FUNCPTR(gst_async_netclock_bin_provide_clock);

	gst_element_class_set_static_metadata(
		element_class,
		"asyncnetclockbin",
		"Generic/Bin/Audio",
		"example source bin with asynchronous netclientclock synchronization",
		"Carlos Rafael Giani <dv@pseudoterminal.org>"
	);

	g_object_class_install_property(
		object_class,
		PROP_TIME_SOURCE_ADDRESS,
		g_param_spec_string(
			"time-source-address",
			"Time source address",
			"IP addres of the host where a nettimeprovider is running",
			DEFAULT_TIME_SOURCE_ADDRESS,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
		)
	);

	g_object_class_install_property(
		object_class,
		PROP_TIME_SOURCE_PORT,
		g_param_spec_uint(
			"time-source-port",
			"Time source port",
			"UDP port for incoming nettimeprovider data",
			1, 65535,
			DEFAULT_TIME_SOURCE_PORT,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
		)
	);
}


static void gst_async_netclock_bin_init(GstAsyncNetclockBin *async_netclock_bin)
{
	GstElement *src_element;
	GstPad *pad;

	GST_OBJECT_FLAG_SET(async_netclock_bin, GST_ELEMENT_FLAG_PROVIDE_CLOCK);
	GST_OBJECT_FLAG_SET(async_netclock_bin, GST_ELEMENT_FLAG_SOURCE);

	async_netclock_bin->netclock = NULL;
	async_netclock_bin->time_source_address = NULL;
	async_netclock_bin->time_source_port = DEFAULT_TIME_SOURCE_PORT;

	src_element = gst_element_factory_make("audiotestsrc", NULL);
	g_assert(src_element != NULL);
	g_object_set(G_OBJECT(src_element), "is-live", TRUE, "samplesperbuffer", 4096, "do-timestamp", TRUE, NULL);
	gst_bin_add(GST_BIN(async_netclock_bin), src_element);

	pad = gst_element_get_static_pad(src_element, "src");
	gst_element_add_pad(GST_ELEMENT(async_netclock_bin), gst_ghost_pad_new("src", pad));
	gst_object_unref(GST_OBJECT(pad));
}


static void gst_async_netclock_bin_dispose(GObject *object)
{
	GstAsyncNetclockBin *async_netclock_bin = GST_ASYNC_NETCLOCK_BIN(object);

	if (async_netclock_bin->netclock != NULL)
	{
		gst_object_unref(async_netclock_bin->netclock);
		async_netclock_bin->netclock = NULL;
	}

	g_free(async_netclock_bin->time_source_address);
	async_netclock_bin->time_source_address = NULL;

	G_OBJECT_CLASS(gst_async_netclock_bin_parent_class)->dispose(object);
}


static void gst_async_netclock_bin_set_property(GObject *object, guint prop_id, GValue const *value, GParamSpec *pspec)
{
	GstAsyncNetclockBin *async_netclock_bin = GST_ASYNC_NETCLOCK_BIN(object);

	switch (prop_id)
	{
		case PROP_TIME_SOURCE_ADDRESS:
			GST_OBJECT_LOCK(object);
			g_free(async_netclock_bin->time_source_address);
			async_netclock_bin->time_source_address = g_value_dup_string(value);
			GST_OBJECT_UNLOCK(object);
			break;

		case PROP_TIME_SOURCE_PORT:
			GST_OBJECT_LOCK(object);
			async_netclock_bin->time_source_port = g_value_get_uint(value);
			GST_OBJECT_UNLOCK(object);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}


static void gst_async_netclock_bin_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GstAsyncNetclockBin *async_netclock_bin = GST_ASYNC_NETCLOCK_BIN(object);

	switch (prop_id)
	{
		case PROP_TIME_SOURCE_ADDRESS:
			GST_OBJECT_LOCK(object);
			g_value_set_string(value, async_netclock_bin->time_source_address);
			GST_OBJECT_UNLOCK(object);
			break;

		case PROP_TIME_SOURCE_PORT:
			GST_OBJECT_LOCK(object);
			g_value_set_uint(value, async_netclock_bin->time_source_port);
			GST_OBJECT_UNLOCK(object);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}


static GstStateChangeReturn gst_async_netclock_bin_change_state(GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret;
	GstAsyncNetclockBin *async_netclock_bin = GST_ASYNC_NETCLOCK_BIN(element);

	switch (transition)
	{
		case GST_STATE_CHANGE_NULL_TO_READY:
		{
			GstMessage *message;

			GST_OBJECT_LOCK(element);
			async_netclock_bin->netclock = gst_net_client_clock_new(
				"example-net-clock",
				async_netclock_bin->time_source_address,
				async_netclock_bin->time_source_port,
				0
			);
			async_netclock_bin->async_pending = TRUE;
			GST_OBJECT_UNLOCK(element);

			g_signal_connect(G_OBJECT(async_netclock_bin->netclock), "synced", G_CALLBACK(gst_async_netclock_bin_on_clock_synced), async_netclock_bin);

			GST_INFO_OBJECT(async_netclock_bin, "starting async state change, finishes when netclientclock is synchronized");
			message = gst_message_new_async_start(GST_OBJECT_CAST(element));
			GST_BIN_CLASS(gst_async_netclock_bin_parent_class)->handle_message(GST_BIN_CAST(element), message);

			break;
		}

		default:
			break;
	}

	if ((ret = GST_ELEMENT_CLASS(gst_async_netclock_bin_parent_class)->change_state(element, transition)) == GST_STATE_CHANGE_FAILURE)
		return ret;

	switch (transition)
	{
		case GST_STATE_CHANGE_NULL_TO_READY:
		{
			ret = GST_STATE_CHANGE_ASYNC;
			break;
		};

		case GST_STATE_CHANGE_PAUSED_TO_READY:
		{
			if (async_netclock_bin->netclock != NULL)
			{
				gst_object_unref(async_netclock_bin->netclock);
				async_netclock_bin->netclock = NULL;
			}

			break;
		}

		default:
			break;
	}

	return ret;
}


static GstClock* gst_async_netclock_bin_provide_clock(GstElement *element)
{
	GstAsyncNetclockBin *async_netclock_bin = GST_ASYNC_NETCLOCK_BIN_CAST(element);
	gst_object_ref(GST_OBJECT_CAST(async_netclock_bin->netclock));
	return async_netclock_bin->netclock;	
}


static void gst_async_netclock_bin_on_clock_synced(GstClock *clock, gboolean synced, gpointer user_data)
{
	GstAsyncNetclockBin *async_netclock_bin = GST_ASYNC_NETCLOCK_BIN_CAST(user_data);

	if (synced && async_netclock_bin->async_pending)
	{
		GST_INFO_OBJECT(async_netclock_bin, "netclientclock is synchronized, finishing async state change");

		GstMessage *message = gst_message_new_async_done(GST_OBJECT_CAST(async_netclock_bin), GST_CLOCK_TIME_NONE);
		GST_BIN_CLASS(gst_async_netclock_bin_parent_class)->handle_message(GST_BIN_CAST(async_netclock_bin), message);
		async_netclock_bin->async_pending = FALSE;
	}
}
