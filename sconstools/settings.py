import re

from SCons.Script.SConscript import SConsEnvironment
from SCons.SConf import CheckContext
from SCons.Util import WhereIs

import pkgconfig
from version import version

def symbolname(name):
    return name.upper().replace("+", "_").replace("-", "_").replace(".", "")

class defines(dict):
    """A class that collects configuration settings to be used when
    compiling the source code.
    """
    def tostrings(self, format, intformat):
        out = []
        for key, val in self.items():
            if type(val) == str:
                out.append(format % (key, val))
            elif type(val) in (int, bool):
                out.append(intformat % (key, val))
            else:
                raise TypeError
        return out
    
    def toswitches(self):
        return self.tostrings('-D%s=\\""%s"\\"', '-D%s=%d')

    def tofile(self, filename):
        lines = self.tostrings("#define %s \"%s\"", "#define %s %d")
        open(filename, "w").write("\n".join(lines) + "\n")

    def add(self, name, value = True):
        self[name] = value

    # Some magic to make this class behave almost like a dictionary.
    def __getattr__(self, attr):
        try:
            return self[attr]
        except KeyError:
            raise AttributeError, attr

    def __setitem__(self, key, value):
        dict.__setitem__(self, symbolname(key), value)

    def __setattr__(self, attr, value):
        self[attr] = value

    # Convenience methods that sets miscallenious stuff.
    def addpkgmodule(self, name, version):
        self.add("HAVE_" + name)
        self.add(name + "_MAJOR_VERSION", version.major)
        self.add(name + "_MINOR_VERSION", version.minor)
        self.add(name + "_MICRO_VERSION", version.micro)

    def addinstallpaths(self, paths):
        for attr, value in vars(paths).items():
            self.add(attr, value)

    def set_gnome_maintainermode(self):
        """Call this method to indicate that this object defines a
        maintainer build?
        """
        gnomelibs = "G", "GDK", "GTK", "GNOME", "PANGO", "BONOBO"
        for gnomelib in gnomelibs:
            self.add(gnomelib + "_DISABLE_DEPRECATED")

def failif(critical, result, msg):
    if not result:
        print msg
        if critical:
            Exit(1)

class configurator:
    def __init__(self, env, defs, lang = "c"):
        self.conf = SConsEnvironment.Configure(env)
        self.ctx = CheckContext(self.conf)
        self.defs = defs
        self.lang = lang
        self.env = env
        
    def ANSICHeaders(self, critical = True):
        """Note: This test only tests for the existance of some ANSI C
        header files. It is not as thorough as AC_HEADER_STDC.
        """
        cHeadersSource = """
        #include <stdlib.h>
        #include <stdarg.h>
        #include <string.h>
        #include <float.h> int main() { return 0; }"""

        self.ctx.Message('Checking for ANSI C header files ... ')
        result = self.ctx.TryCompile(cHeadersSource, ".c")
        self.ctx.Result(result)
        self.defs.add("HAVE_STDC_HEADERS")

        failif(critical, result, "Couldn't detect ANSI C headers.")
        return result

    def Func(self, func, critical = True):
        result = self.conf.CheckFunc(func)
        self.defs.addHaveSetting(func, True)
        failif(critical, result, "Function %s was not found." % func)
        return result

    def Header(self, header, critical = True):
        result = self.conf.CheckHeader(header, language = self.lang)
        self.defs.addHaveSetting(header, True)
        failif(critical, result, "Header file %s was not found." % header)
        return result

    def Library(self, libname, symbol = "main", critical = True):
        result = self.conf.CheckLib(libname, symbol)
        self.defs.addHaveSetting("LIB" + libname, True)
        failif(critical, result, "Library %s does not provide %s." % \
               (libname, symbol))
        return result

    def PkgCheckModules(self, pkgreqs, critical = True):
        if not self.defs.has_key("HAVE_PKG_CONFIG"):
            has_pkg = self.CheckProgram("pkg-config", critical)
            self.defs.add("HAVE_PKG_CONFIG", has_pkg)

        self.ctx.Message("Checking for pkg-config requirements %s ... " \
                         % " ".join(pkgreqs))
        failures = []
        for dependancy in pkgreqs:
            modname, atleastversion = pkgconfig.depfromstring(dependancy)
            try:
                pkgconfig.fetchdep(modname, atleastversion,
                                   self.env, self.defs)
            except pkgconfig.DepNotMet, reason:
                failures.append(reason)

        self.ctx.Result(not failures)
        if failures and critical:
            util.error(*failures)
        elif failures:
            util.warning(*failures)
        return not failures

    def CheckProgram(self, program, critical = True, msg = None):
        """Check where binary program is. If found, add the path of
        the program as a configuration setting.
        """
        self.ctx.Message("Checking for %s ... " % program)
        result = WhereIs(program)

        # This code is weird. Result() can't handle the None that
        # WhereIs() returns if the program is not found. Therefore, if
        # None is returned it is converted to False. I think it is a
        # bug in scons.
        self.ctx.Result(result != None)
            
        failif(critical, result, msg or "Couldn't find %s." % program)
        self.defs.add(program, result)
        return result

    def CheckIntltool(self, req_ver, critical = True):
        """Use this check if you depend on any of the intltool-*
        utilities.
        """
        self.ctx.Message("Checking for intltool >= %s ... " % req_ver)
        
        progfile = WhereIs("intltool-update")
        if not progfile:
            failif(critical, False, "Couldn't find intltool-update")
            self.ctx.Result(False)
            return False

        # Find the value of the $VERSION variable inside the
        # intltool-update's Perl code.
        contents = open(progfile).read()
        regex = 'my\s+\$VERSION\s+=\s+["\']([0-9.]+)["\']'
        match = re.search(regex, contents)
        if not match:
            failif(critical, False, "Couldn't determine intltool's " \
                   "version from intltool-update")
            self.ctx.Result(False)
            return False

        applied_version = version.fromstring(match.groups()[0])
        result = applied_version >= version.fromstring(req_ver)
        
        self.ctx.Result(result)
        failif(critical, result,
               "Installed version is older than required version")
        return result
    

if __name__ == "__main__":
    defs = defines()
    defs["G_DISABLE_DEPRECATED"] = True
    assert(defs.G_DISABLE_DEPRECATED == True)
    try:
        defs.foobar == "Yes"
    except AttributeError:
        raised = True
    assert(raised == True)
    defs.GDK_DISABLE_DEPRECATED = False
    assert(defs.GDK_DISABLE_DEPRECATED == False)
    defs.moo = 1234
    assert(defs.MOO == 1234)
    defs["hi"] = "Foo"
    assert(defs["HI"] == "Foo")
    defs.add("LIBTOOL_MINOR")
    assert(defs.LIBTOOL_MINOR == True)

    onlyone = defines({"FOOBAR" : 1})
    assert onlyone.toswitches() == ["-DFOOBAR=1"]

    onlyone.tofile("foobar")

    

