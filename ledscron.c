#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/XKBlib.h>
#include <sys/select.h>

#define OBJECT_PATH    "/com/github/liaonau/"NAME
#define INTERFACE_NAME "com.github.liaonau."NAME
#define METHOD_NAME    "Get"
#define SIGNAL_NAME    "Changed"

#define SLEEP_SECS 5

static Display  *display  = NULL;
static int xkb_event_base = 0;
static int xkb_error_base = 0;

static GDBusNodeInfo *introspection_data = NULL;
static const gchar introspection_xml[] =
"<node>"
"    <interface name='"INTERFACE_NAME"'>"
"        <signal name='"SIGNAL_NAME"'>"
"            <arg type='u' name='mods' direction='out'/>"
"        </signal>"
"        <method name='"METHOD_NAME"'>"
"            <arg type='u' name='mods' direction='out'/>"
"        </method>"
"    </interface>"
"</node>";

static void fatal(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

gpointer listener(gpointer data)
{
    GDBusConnection* connection = (GDBusConnection*)data;
    int x11_fd = ConnectionNumber(display);
    XkbEvent ev;

    fd_set fds;
    struct timeval tv = {.tv_sec = 0, .tv_usec = 1};

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(x11_fd, &fds);

        int num_ready_fds = select(x11_fd + 1, &fds, NULL, NULL, &tv);
        if (num_ready_fds < 0)
            fatal("an error on select");
        /*else if (num_ready_fds == 0)*/
            /*printf("timer\n");*/

        while (XPending(display))
        {
            XNextEvent(display, &ev.core);
            if (ev.type == xkb_event_base && ev.any.xkb_type == XkbIndicatorStateNotify)
            {
                unsigned int s;
                XkbGetIndicatorState(display, XkbUseCoreKbd, &s);
                GError *er = NULL;
                g_dbus_connection_emit_signal(connection, NULL, OBJECT_PATH, INTERFACE_NAME, SIGNAL_NAME,
                        g_variant_new("(u)", ev.indicators.state), &er);
                g_assert_no_error(er);
            }
        }

        tv.tv_usec = 0;
        tv.tv_sec  = SLEEP_SECS;
    }
    return NULL;
}

static void method_handler(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name,
        const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
    if (g_strcmp0(method_name, METHOD_NAME) != 0)
        return;
    unsigned int state;
    if (XkbGetIndicatorState(display, XkbUseCoreKbd, &state) != Success)
        fatal("error reading indicators state");
    g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", state));
}
static const GDBusInterfaceVTable iface_vtable = { method_handler, NULL, NULL, {NULL}, };
static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    guint id = g_dbus_connection_register_object(connection, OBJECT_PATH, introspection_data->interfaces[0], &iface_vtable, NULL, NULL, NULL);
    g_assert(id > 0);
}
static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    g_thread_new(NULL, listener, connection);
}
static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    fatal(NAME" lost name on bus");
}

int main(int argc, char *argv[])
{
    if (!XInitThreads())
        fatal("cannot initialize X threads");
    int maj = XkbMajorVersion;
    int min = XkbMinorVersion;
    int opcode;

    display = XOpenDisplay(NULL);
    if (!display)
        fatal("unable to get Display");
    if (!XkbLibraryVersion(&maj, &min))
        fatal("unable to get xkb library version");
    if (!XkbQueryExtension(display, &opcode, &xkb_event_base, &xkb_error_base, &maj, &min))
        fatal("XkbQueryExtension error");
    if (!XkbSelectEvents(display, XkbUseCoreKbd, XkbIndicatorStateNotifyMask, XkbIndicatorStateNotifyMask))
        fatal("XkbSelectEvents error");

    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    g_assert(introspection_data != NULL);

    int type  = G_BUS_TYPE_SESSION;
    int flags = G_BUS_NAME_OWNER_FLAGS_NONE;
    gint id   = g_bus_own_name(type, INTERFACE_NAME, flags, on_bus_acquired, on_name_acquired, on_name_lost, NULL, NULL);

    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    //We'll never reach here.
    g_bus_unown_name(id);
    g_dbus_node_info_unref(introspection_data);
    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
