import os
from glob import glob
from os.path import isfile, join

from sconstools.builders import buildfeature, cbuild, configh, \
     pobuild, desktopfile, datainstall, template

class notifycheck(buildfeature):
    """Whether to build with libnotify support or not. 'auto' means
    means that libnotify support will be automagically detected.
    """
    options = [EnumOption("libnotify", "Build with libnotify support", "auto",
                          ("yes", "no", "auto"))]
    
    def checkit(self, opts, confer, defs):
        opt = opts["libnotify"]
        if opt in ("auto", "yes"):
            confer.PkgCheckModules(["libnotify >= 0.2"], opt == "yes")

class doxygen(buildfeature):
    options = [BoolOption("doxygen", "Build Doxygen docs (requires Doxygen)",
                          0)]
    def checkit(self, opts, confer, defs):
        if opts["doxygen"]:
            confer.CheckProgram("doxygen", msg = \
                                "Building Doxygen docs explicitly required, "
                                "but Doxygen not found")
        else:
            self.skipbuild = True

    def build(self, env, defs):
        # Build the Doxyfile file using the template builder
        doxyfile = template("docs/Doxyfile.in").build(env, defs)

        # This is certainly a hack. First we add all files in the
        # doxygen directory to ensure that all doxygen files are
        # removed when we cleanup. Then we filter out all directories
        # because scons doesn't like it when directories are targets
        # of Command():s. Then we add a file that we know for sure
        # that doxygen will generate so that the target list is not
        # empty. Because if it is, scons will just ignore the
        # Command().
        doxytargets = [f for f in glob("docs/doxygen/*") if isfile(f)]
        doxytargets += ["docs/doxygen/index.html"]
        env.Command(doxytargets, doxyfile, "cd docs; doxygen")


# We use two custom builders for the glib-genmarshal and the libtool
# --mode=execute targets. If a few GNOME projects use glib-genmarshal,
# then those builders should be added to sconstools/builders.py.
class gpmmarshal(buildfeature):
    def checkit(self, opts, confer, defs):
        confer.CheckProgram("glib-genmarshal")

    def build(self, env, defs):
        env.Command("src/gpm-marshal.h", "src/gpm-marshal.list",
                    "%s --prefix=gpm_marshal $SOURCE --header > "
                    "$TARGET" % defs.GLIB_GENMARSHAL)
        env.Command("src/gpm-marshal.c", "src/gpm-marshal.list",
                    "%s --prefix=gpm_marshal $SOURCE --body > "
                    "$TARGET" % defs.GLIB_GENMARSHAL)

class gpmglue(buildfeature):
    def checkit(self, opts, confer, defs):
        confer.CheckProgram("libtool")
        confer.CheckProgram("dbus-binding-tool")

    def build(self, env, defs):
        env.Command("src/gpm-manager-glue.h", "src/gpm-manager.xml",
                    "libtool --mode=execute dbus-binding-tool "
                    "--prefix=gpm_manager "
                    "--mode=glib-server "
                    "--output=$TARGET $SOURCE")

class mycbuild(cbuild):
    def checkit(self, opts, confer, defs):
        # Check stuff needed for a successful compilation.
        confer.ANSICHeaders()
        pkg_deps = ["glib-2.0 >= 2.6.0",
                    "gtk+-2.0 >= 2.6.0",
                    "libgnome-2.0 >= 2.10.0",
                    "libgnomeui-2.0 >= 2.10.0",
                    "libglade-2.0 >= 2.5.0",
                    "libwnck-1.0 >= 2.10.0",
                    "dbus-1 >= 0.50",
                    "dbus-glib-1 >= 0.50",
                    "hal >= 0.5.6",
                    "gthread-2.0"]
        confer.PkgCheckModules(pkg_deps)

        # Make paths availible for source files.
        defs.addinstallpaths(env.paths)
        
env = Environment(ENV = os.environ,
                  toolpath = ["sconstools"],
                  tools = ["default", "gnomebuild"])

env.features.register(notifycheck())

# Whether to use a config.h file or not is specified on the command
# line.
env.features.register(configh())
env.features.register(gpmmarshal())
env.features.register(gpmglue())
env.features.register(pobuild("po"))
env.features.register(desktopfile("data/gnome-power-preferences.desktop.in"))
env.features.register(mycbuild("src", "gnome-power-preferences",
                               ["gpm-hal.c",
                                "gpm-screensaver.c",
                                "gpm-prefs.c",
                                "gpm-debug.c",
                                "gpm-common.c"]))
env.features.register(mycbuild("src", "gnome-power-manager",
                               ["eggtrayicon.c",
                                "gpm-common.c",
                                "gpm-dpms-x11.c",
                                "gpm-hal.c",
                                "gpm-hal-monitor.c",
                                "gpm-idle.c",
                                "gpm-main.c",
                                "gpm-debug.c",
                                "gpm-manager.c",
                                "gpm-marshal.c",
                                "gpm-networkmanager.c",
                                "gpm-brightness.c",
                                "gpm-power.c",
                                "gpm-screensaver.c",
                                "gpm-stock-icons.c",
                                "gpm-tray-icon.c"]))
env.features.register(datainstall(glob("data/icons/24x24/*.png") +
                                  ["data/gpm-prefs.glade"]))

# Print help page and terminate execution if the user requested help.
env.DoHelp()

# Extra verbose warning switches
env.CCFLAGS += "-Wall" , "-Werror"

# Debugging switches
env.CCFLAGS += "-g", "-fexceptions"

# Add some constants passed either via config.h, or via gcc -D
# switches to the source code. These constants are also used by the
# different buildfeatures we have registered.
defs = env.defs
defs.VERSION = "2.13.5"
defs.PACKAGE = defs.GETTEXT_PACKAGE = "gnome-power-manager"

# Add path-derived constants specific to g-p-m.
defs.GPM_DATA = join(env.paths.datadir, "gnome-power-manager")
defs.GNOMELOCALEDIR = join(env.paths.datadir, "locale")

env.features.config()
env.features.build()
