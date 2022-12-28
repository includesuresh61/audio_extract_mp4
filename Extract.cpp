
#define GLIB_DISABLE_DEPRECATION_WARNING
#include<iostream>
#include<string>
extern "C" 
{
#include<sys/wait.h>
#include<gst/gst.h>
#include<gio/gio.h>
#include <gst/net/gstnet.h>
#include <glib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
}
using namespace std;

static GMainLoop *loop;
typedef struct elements{
  GstElement *filesrc,*demux,*audiparse,*videoparse,*queue1,*queue2,*id,*caps;
  GstElement *audiodec,*videodec,*audioconvert,*audioresample,*audioenc,*filesink,*fakesink;
  GstPad *auSinkPad,*viSinkPad;
  GstElement *cappipe;
  string filename;
}ST;
void audio_clipping_pipeline(GstElement *pipeline,ST *data);
static gboolean handle_event(GstBus* bus, GstMessage* msg, gpointer data)
{
  ST *elem = static_cast<ST *>(data);
  char *buf;
  gint64 duration;
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      {
        cout<<"handle_event got GST_MESSAGE_ERROR message"<<endl;
        GError *err = NULL;
        gchar *debug_info = NULL;
        gchar *p;
        gst_message_parse_error (msg, &err, &debug_info);
        g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
        g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
        g_clear_error (&err);
        g_free (debug_info);

        break;
      }
    case GST_MESSAGE_EOS:
      {
          g_print("handle_event got GST_MESSAGE_EOS !!!!\n");
          gst_element_set_state(elem->cappipe,GST_STATE_NULL);
          gst_object_unref (elem->cappipe);
          g_main_loop_quit(loop);
        break;

      }
  }
  return TRUE;
}

int main(int c,char *argv[])
{
  gst_init(NULL,NULL);
  GstElement *pipeline;
  GstBin *m_bin;
  GstStateChangeReturn ret;
  guint busId;
  GstBus *bus;
  GstCaps *cp;
  GstPadLinkReturn isPadLinked;
  ST data;
  
  data.cappipe=pipeline=gst_pipeline_new("audio-clipping-pipeline");
  data.filename="/home/sample-mp4-file.mp4";
  audio_clipping_pipeline(pipeline,&data);
  bus=gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  busId=gst_bus_add_watch(bus, &handle_event,&data);
  
  ret=gst_element_set_state(pipeline,GST_STATE_PLAYING);
  g_print("STARTED the capture pipeline\n");
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  g_source_remove (busId);
  g_main_loop_unref (loop);
  }
static void user_function (GstElement* object,GstPad* pad,gpointer user_data){
  gchar *padName = NULL;
  GstPadDirection padDir = GST_PAD_UNKNOWN;
  GstPad *sinkPad, *peerPad = NULL;
  GstPadLinkReturn isPadLinked;
  ST *data=(ST *)user_data;
  if(!data)return;
  padName = gst_pad_get_name(pad);
  g_print("padName:%s\n",padName);
  if(strstr(padName,"audio")){
     isPadLinked = gst_pad_link(pad,data->auSinkPad);
     if(GST_PAD_LINK_WAS_LINKED == isPadLinked) {
         peerPad = gst_pad_get_peer(data->auSinkPad);
         if(gst_pad_unlink(peerPad,data->auSinkPad)) {
            isPadLinked = gst_pad_link(pad, data->auSinkPad);
         }
     }
    //gst_object_unref(auSinkPad);
    if(isPadLinked != GST_PAD_LINK_OK) {
     g_print("audio pad not linked\n");
    }else{
    gst_element_sync_state_with_parent(data->queue1);
    gst_element_sync_state_with_parent(data->audiparse);
    gst_element_sync_state_with_parent(data->audiodec);
    gst_element_sync_state_with_parent(data->audioconvert);
    gst_element_sync_state_with_parent(data->audioresample);
    gst_element_sync_state_with_parent(data->audioenc);
    gst_element_sync_state_with_parent(data->filesink);
    }
  }else if(strstr(padName,"video")){
    isPadLinked = gst_pad_link(pad,data->viSinkPad);
    if(GST_PAD_LINK_WAS_LINKED == isPadLinked) {
      peerPad = gst_pad_get_peer(data->viSinkPad);
      if(gst_pad_unlink(peerPad,data->viSinkPad)) {
        isPadLinked = gst_pad_link(pad,data->viSinkPad);
      }
    }
    //gst_object_unref(viSinkPad);
    if(isPadLinked != GST_PAD_LINK_OK) {
      g_print("audio pad not linked\n");
    }else{
      gst_element_sync_state_with_parent(data->videoparse);
      gst_element_sync_state_with_parent(data->videodec);
      gst_element_sync_state_with_parent(data->queue2);
      gst_element_sync_state_with_parent(data->fakesink);
    }
  }else
   g_print("UNKOWN pad\n");

   GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data->cappipe), GST_DEBUG_GRAPH_SHOW_ALL,"audio-clip-channel");
  return;
}
static gboolean identity_callback(GstElement * identity,GstBuffer * buffer,gpointer data){
  g_print("identity_callback=----\n");
  return TRUE;
}
void audio_clipping_pipeline(GstElement *pipeline,ST *data){
  GstBin *m_bin;
  GstCaps *cp;
  GstElement *filesrc,*decBin,*demux,*audiparse,*videoparse,*queue1,*queue2;
  GstElement *audiodec,*videodec,*audioconvert,*audioresample,*audioenc,*filesink;

   data->filesrc=filesrc=gst_element_factory_make("filesrc",NULL);
   g_object_set(G_OBJECT(data->filesrc),"location",data->filename.c_str(),NULL);
   data->demux=gst_element_factory_make("qtdemux",NULL);
   g_signal_connect(G_OBJECT(data->demux),"pad-added",G_CALLBACK(&user_function), data);
   data->audiparse=gst_element_factory_make("aacparse",NULL);
   data->videoparse=gst_element_factory_make("h264parse",NULL);
   data->queue1=gst_element_factory_make("queue",NULL);
   data->queue2=gst_element_factory_make("queue",NULL);
   data->audiodec=gst_element_factory_make("fdkaacdec",NULL);
   data->videodec=gst_element_factory_make("openh264dec",NULL);
   data->audioconvert=gst_element_factory_make("audioconvert",NULL);
   data->audioresample=gst_element_factory_make("audioresample",NULL);
   data->audioenc=gst_element_factory_make("wavenc",NULL);
   data->filesink=gst_element_factory_make("filesink",NULL);
   data->fakesink=gst_element_factory_make("fakesink",NULL);
   g_object_set(G_OBJECT(data->filesink),"location","myaudio.wav",NULL);

  if(!data->filesrc || !data->demux || !data->audiparse || !data->videoparse || !data->queue1 || !data->queue2 || !data->audiodec || !data->videodec || !data->audioconvert || !data->audioresample || !data->audioenc || !data->filesink || !data->fakesink){
    cout<<" element creation failed!!"<<endl;
    return;
  }
 /*data->id=gst_element_factory_make("identity",NULL);
 g_object_set(G_OBJECT(data->id),"signal-handoffs",TRUE,NULL);
 g_object_set(G_OBJECT(data->id),"signal-handoffs",TRUE,NULL);
 g_signal_connect(G_OBJECT(data->id),"handoff",G_CALLBACK(&identity_callback), data);
 
 
 data->caps=gst_element_factory_make("capsfilter",NULL);
 g_object_set(G_OBJECT(data->caps),"caps-change-mode",0,NULL);
 cp=gst_caps_from_string("audio/x-raw,clock-rate=(int)8000");
 g_object_set(G_OBJECT(data->caps),"caps",cp,NULL);
 gst_caps_unref(cp);*/

 data->auSinkPad=gst_element_get_static_pad(data->queue1,"sink");
 data->viSinkPad=gst_element_get_static_pad(data->queue2,"sink");

  m_bin = GST_BIN(gst_bin_new("ClipAudioBin"));
  gst_bin_add_many(m_bin,data->filesrc,data->demux,data->audiparse,data->videoparse,data->queue1,data->queue2,data->audiodec,\
   data->videodec,data->audioconvert,data->audioresample,data->audioenc,data->filesink,data->fakesink,NULL);
  gst_element_link_many(data->filesrc,data->demux,NULL);
  gst_element_link_many(data->queue1,data->audiparse,data->audiodec,data->audioconvert,data->audioresample,data->audioenc,data->filesink,NULL);
  gst_element_link_many(data->queue2,data->videoparse,data->videodec,data->fakesink,NULL);
  
  gst_bin_add_many(GST_BIN(data->cappipe),GST_ELEMENT(m_bin),NULL);

}



