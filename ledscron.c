#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/XKBlib.h>

#define OBJECT_PATH    "/com/github/liaonau/"NAME
#define INTERFACE_NAME "com.github.liaonau."NAME
#define SIGNAL_NAME    "Changed"

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
"    </interface>"
"</node>";

static void fatal(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static const GDBusInterfaceVTable iface_vtable = { NULL, NULL, NULL, {NULL}, };
static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    guint id = g_dbus_connection_register_object(connection, OBJECT_PATH, introspection_data->interfaces[0], &iface_vtable, NULL, NULL, NULL);
    g_assert(id > 0);
}
static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    XkbEvent ev;
    while (1)
    {
        XNextEvent(display, &ev.core);
        if (ev.type == xkb_event_base && ev.any.xkb_type == XkbIndicatorStateNotify)
        {
            unsigned int state = ev.indicators.state;
            GError *er = NULL;
            g_dbus_connection_emit_signal(connection, NULL, OBJECT_PATH, INTERFACE_NAME, SIGNAL_NAME, g_variant_new("(u)", state), &er);
            g_assert_no_error(er);
        }
    }
}
static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    fatal("lost name on bus");
}

static void start_dbus(void)
{
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
}
static void init_display(void)
{
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
}

int main(int argc, char *argv[])
{
    init_display();
    start_dbus();

    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
