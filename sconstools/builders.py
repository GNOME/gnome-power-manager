import os
from glob import glob
from os.path import basename, join, splitext

from SCons.Options import BoolOption
from SCons.Errors import UserError

class buildfeature:
    options = []
    skipbuild = False
    checked = False
    
    def checkit(self, opts, confer, defs):
        pass

    def build(self, env, defs):
        pass

class template(buildfeature):
    def __init__(self, input):
        """FillTemplate() replaces all instances of variables like
        @FOOBAR@ in 'input' with their values gotten from the defs
        dictionary (in the build step). I.e: @FOOBAR@ is replaced with
        ENV.defs.FOOBAR. The output file is the input file with the
        ending '.in' removed.
        """
        self.input = input

    def build(self, env, defs):
        def build_it(target, source, env):
            source_path = source[0].path
            target_path = target[0].path
            output = self.fill(open(source_path).read(), defs)
            open(target_path, "w").write(output)
            
        return env.Command(splitext(self.input)[0], self.input, build_it)[0]
        
    def fill(self, text, dict):
        out = ""
        mode = "text"
        for ch in text:
            if mode == "text":
                if ch == "@":
                    mode = "symbol"
                    varname = ""
                else:
                    out += ch
            elif mode == "symbol":
                if ch in ("@", " ", ",", "$"):
                    mode = "text"
                    if ch == "@":
                        out += str(dict[varname])
                    else:
                        out += "@" + varname + ch
                else:
                    varname += ch
        return out

class desktopfile(buildfeature):
    options = [BoolOption("desktopfile", "Build and install desktop file", 1)]

    def __init__(self, source):
        self.source = source

    def checkit(self, opts, confer, defs):
        if opts["desktopfile"]:
            confer.CheckIntltool("0.27")
        else:
            self.skipbuild = True

    def build(self, env, defs):
        intl_merge_cmd = 'LC_ALL=C intltool-merge ' \
                         '--desktop --utf8 ' \
                         '--cache=./po/.intltool-merge-cache po/ ' \
                         '$SOURCE $TARGET'
        desktop_out = splitext(self.source)[0]
        desktop_out = env.Command(desktop_out, self.source, intl_merge_cmd)
        env.Alias("install", env.Install(env.paths.appdir, desktop_out))

class configh(buildfeature):
    options = [BoolOption("configh", "Use a config.h file to build", 1)]

    def checkit(self, opts, confer, defs):
        self.useconfigh = opts["configh"]

    def build(self, env, defs):
        """Make CCFLAGS of all defines in the defs object. Either
        output them as command line switches, or build a config.h
        file.
        """
        if not self.useconfigh:
            env.AppendUnique(CCFLAGS = env.defs.toswitches())
        else:
            def config_h_build(target, source, env):
                env.defs.tofile(str(target[0]))
            env.Command("config.h", "options.cache", config_h_build)
            env.AppendUnique(CPPPATH = ["."])
            env.AppendUnique(CCFLAGS = ["-DHAVE_CONFIG_H"])

class pobuild(buildfeature):
    options = [("linguas", "A string specifying which locales to use", "all")]
    
    def __init__(self, dir):
        """Builds directory 'dir' containing po-files to GNU message
        catalog files.
        """
        self.dir = dir
    
    def checkit(self, opts, confer, defs):
        confer.CheckProgram("msgfmt")
        try:
            defs.GNOMELOCALEDIR
            defs.GETTEXT_PACKAGE
        except AttributeError:
            raise UserError("Usage of the PoBuild builder requires that "
                            "GNOMELOCALEDIR and GETTEXT_PACKAGE are defined")
        self.locales = self.desired_locales(opts["linguas"])

    def build(self, env, defs):
        for po in self.locales:
            lang = splitext(basename(po))[0]
            gmo = env.Command(po.replace(".po", ".gmo"), po,
                              "msgfmt -o $TARGET $SOURCE")
            
            modir = join(defs.GNOMELOCALEDIR, lang, "LC_MESSAGES")
            moname = defs.GETTEXT_PACKAGE + ".mo"
            env.Alias("install", env.InstallAs(join(modir, moname), gmo))


    def desired_locales(self, linguasopt):
        all_locales = glob(self.dir + "/*.po")
        if linguasopt == "all":
            return all_locales
        desired_langs = linguasopt.split()
        desired_locales = []
        for locale in all_locales:
            lang = splitext(basename(locale))[0]
            if lang in desired_langs:
                desired_locales.append(locale)
        return desired_locales

class cbuild(buildfeature):
    def __init__(self, dir, target = None, sources = None):
        """Initialize a cbuild to build all 'sources' in directory
        'dir' to a binary named 'target'. Each C-file is compiled with
        those switches that was determined in the configure step of
        the build and exported to a config.h or to the command
        line. Therefore, this builder depends on configh.
        """
        if sources is None:
            self.sources = [join(dir, src) for src in os.listdir(dir) \
                            if self.issource(src)]
        else:
            self.sources = [join(dir, src) for src in sources]
        if not self.sources:
            raise UserError("Build directory '%s' doesn't contain any C "
                            "source code." % dir)
        self.target = target
        self.dir = dir

    def issource(self, name):
        return name.endswith(".c") or name.endswith(".cc")

    def build(self, env, defs):
        # Add user defines CCFLAGS
        env.AppendUnique(CCFLAGS = list(env.CCFLAGS))

        if self.target:
            binary = env.Program(join(self.dir, self.target), self.sources)
        else:
            binary = env.Program(self.sources)
        env.Alias("install", env.Install(env.paths.bindir, binary))

class datainstall(buildfeature):
    def __init__(self, sources):
        self.sources = sources

    def checkit(self, opts, confer, defs):
        try:
            defs.PACKAGE
        except AttributeError:
            raise UserError("Usage of 'datainstall' requires that "
                            "the PACKAGE path is specified.")

    def build(self, env, defs):
        destdir = join(env.paths.datadir, defs.PACKAGE)
        for filename in self.sources:
            env.Alias("install", env.Install(destdir, filename))

class iconinstall(buildfeature):
    def __init__(self, icon, reldir):
        """Installs icon in the path specified by
        paths.datadir/reldir. This builder may be redundant.
        """
        self.icon = icon
        self.reldir = reldir

    def build(self, env, defs):
        targ = env.Install(join(env.paths.datadir, self.reldir), self.icon)
        env.Alias("install", targ)
