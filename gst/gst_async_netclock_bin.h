#ifndef GST_ASYNC_NETCLOCK_BIN_H
#define GST_ASYNC_NETCLOCK_BIN_H

#include <stdio.h>
#include <gst/gst.h>
#include <gst/net/net.h>


#define GST_TYPE_ASYNC_NETCLOCK_BIN             (gst_async_netclock_bin_get_type())
#define GST_ASYNC_NETCLOCK_BIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ASYNC_NETCLOCK_BIN, GstAsyncNetclockBin))
#define GST_ASYNC_NETCLOCK_BIN_CAST(obj)        ((GstAsyncNetclockBin *)(obj))
#define GST_ASYNC_NETCLOCK_BIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ASYNC_NETCLOCK_BIN, GstAsyncNetclockBinClass))
#define GST_ASYNC_NETCLOCK_BIN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_ASYNC_NETCLOCK_BIN, GstAsyncNetclockBinClass))
#define GST_IS_ASYNC_NETCLOCK_BIN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ASYNC_NETCLOCK_BIN))
#define GST_IS_ASYNC_NETCLOCK_BIN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ASYNC_NETCLOCK_BIN))


G_BEGIN_DECLS


typedef struct _GstAsyncNetclockBin GstAsyncNetclockBin;
typedef struct _GstAsyncNetclockBinClass GstAsyncNetclockBinClass;


struct _GstAsyncNetclockBin
{
	GstBin parent;
	GstClock *netclock;
	gboolean async_pending;

	gchar *time_source_address;
	guint time_source_port;
};


struct _GstAsyncNetclockBinClass
{
	GstBinClass parent_class;
};


GType gst_async_netclock_bin_get_type(void);


G_END_DECLS


#endif
