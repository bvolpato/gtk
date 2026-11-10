#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>

typedef struct
{
  GdkWindow *window;
  Atom message;
  guint calls;
  guint later_calls;
} TestState;

static GdkFilterReturn
on_event (GdkXEvent *native_event,
          GdkEvent  *event,
          gpointer   data)
{
  XEvent *xevent = native_event;
  TestState *state = data;

  if (xevent->type == ClientMessage &&
      xevent->xclient.message_type == state->message)
    {
      state->calls++;
      gdk_window_destroy (state->window);
    }

  return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
on_later_event (GdkXEvent *native_event,
                GdkEvent  *event,
                gpointer   data)
{
  XEvent *xevent = native_event;
  TestState *state = data;

  if (xevent->type == ClientMessage &&
      xevent->xclient.message_type == state->message)
    state->later_calls++;

  return GDK_FILTER_CONTINUE;
}

static void
test_destroy_window (void)
{
  GdkWindowAttr attributes = { 0 };
  TestState state = { 0 };
  GdkDisplay *display = gdk_display_get_default ();
  Display *xdisplay;
  XEvent event = { 0 };
  gint64 deadline;

  if (!GDK_IS_X11_DISPLAY (display))
    {
      g_test_skip ("X11 display required");
      return;
    }

  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  state.message = XInternAtom (xdisplay, "_GDK_WINDOW_LIFETIME", False);

  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.width = 10;
  attributes.height = 10;
  state.window = gdk_window_new (gdk_get_default_root_window (), &attributes, 0);
  g_object_ref (state.window);
  gdk_window_add_filter (state.window, on_event, &state);
  gdk_window_add_filter (state.window, on_later_event, &state);

  event.xclient.type = ClientMessage;
  event.xclient.window = GDK_WINDOW_XID (state.window);
  event.xclient.message_type = state.message;
  event.xclient.format = 32;
  g_assert_true (XSendEvent (xdisplay, event.xclient.window, False, NoEventMask, &event));
  XSync (xdisplay, False);

  deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  while (state.calls == 0 && g_get_monotonic_time () < deadline)
    {
      if (!g_main_context_iteration (NULL, FALSE))
        g_usleep (1000);
    }

  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_cmpuint (state.later_calls, ==, 0);
  g_object_unref (state.window);
}
#endif

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gdk_init (&argc, &argv);

#ifdef GDK_WINDOWING_X11
  g_test_add_func ("/lifetime/destroy-window", test_destroy_window);
#endif

  return g_test_run ();
}
