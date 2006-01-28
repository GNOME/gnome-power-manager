from subprocess import Popen, PIPE

from version import version

class DepNotMet(Exception):
    pass

class DepNotFound(DepNotMet):
    pass

class DepToOld(DepNotMet):
    pass

class NoPkgConfig(Exception):
    pass

def checkdep(name, atleastversion):
    args = ["pkg-config", "--modversion", '%s' % name]
    try:
        pkgproc = Popen(args, stdout = PIPE, stderr = PIPE)
    except OSError:
        raise NoPkgConfig, "pkg-config not found!"
    pkgproc.wait()
    if pkgproc.returncode != 0:
        raise DepNotFound, "%s not installed" % name
    realversion = version.fromstring(pkgproc.stdout.read())
    if realversion < atleastversion:
        raise DepToOld, "required %s %s but installed is %s" \
              % (name, atleastversion, realversion)
    return realversion

def fetchdep(name, atleastversion, env = None, defs = None):
    realversion = checkdep(name, atleastversion)
    if env:
        env.ParseConfig("pkg-config --cflags --libs \"%s\"" % name)
    if defs:
        defs.addpkgmodule(name, realversion)

def depfromstring(str):
    parts = str.split()
    name = parts[0]
    atleastversion = version(0)
    if len(parts) == 3:
        atleastversion = version.fromstring(parts[2])
    return name, atleastversion
