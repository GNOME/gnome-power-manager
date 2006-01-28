import os
from os.path import join
import sys

# SCons import
from SCons.Script import ARGUMENTS
from SCons.Options import Options, PathOption

# Sibling imports
import settings

# All methods and classes share the same Environment. It is my hope
# that you should not need to use multiple environments to build
# anything and therefore the Environment is global instead of being
# passed around.
ENV = None

def DoHelp():
    """If helping, print help text and immidiately terminate.
    """
    # Save() has to be called here.
    ENV.opts.Save("options.cache", ENV)

    if "-h" in sys.argv or "--help" in sys.argv or "help" in sys.argv:
        print ENV.opts.GenerateHelpText(ENV)
        ENV.Exit(0)

def AddOptions(*optlist):
    """Add a number of options to the Options referenced by ENV.opts
    and update the environment. 

    This function wraps Options.AddOptions.
    """
    ENV.opts.AddOptions(*optlist)
    ENV.opts.Update(ENV)

def setupOptions():
    """Create an Options object attached to ENV.opts. Add all default
    options (only prefix and destdir so far).
    """
    ENV.opts = Options(['options.cache'], ARGUMENTS)
    AddOptions(PathOption('prefix', 'Installation prefix', '/usr/local',
                          PathOption.PathIsDirCreate),
               PathOption('destdir', 'System root directory', '/',
                          PathOption.PathIsDirCreate))

def addInstallPaths():
    """Adds a data container object paths to the Environment. It
    contains the set of paths to which stuff is installed.
    
    paths.prefix is the equivalent of the ./configure-switch
    --prefix. The other paths are derived from that one.
    """
    class placeholder:
        pass

    paths = placeholder()
    paths.destdir    = ENV["destdir"]
    
    # I havent found a path-join function for two absolute
    # paths. os.path.join() is bugged :(
    paths.prefix     = paths.destdir + ENV["prefix"]
    paths.bindir     = join(paths.prefix, "bin")
    paths.libdir     = join(paths.prefix, "lib")
    paths.datadir    = join(paths.prefix, "share")
    paths.appdir     = join(paths.datadir, "applications")
    paths.sysconfdir = join(paths.destdir, "etc")
    ENV.paths = paths

class cflagslist(list):
    def addunique(self, item):
        if item not in self:
            self.append(item)
        
    def __iadd__(self, other):
        if iter(other) and not isinstance(other, basestring):
            for item in other:
                self.addunique(item)
        else:
            self.addunique(other)
        return self

class featuremanager(list):
    def __init__(self, env, defs):
        self.confer = settings.configurator(env, defs)
        self.defs = defs
        self.env = env
    
    def register(self, feature):
        self.append(feature)
        for opt in feature.options:
            AddOptions(opt)

    def config(self):
        for feature in self:
            if not feature.checked:
                optdict = self.getoptionvalues(feature)
                feature.checkit(optdict, self.confer, self.defs)
                feature.__class__.checked = True

    def getoptionvalues(self, feature):
        optvalues = {}
        for opt in feature.options:
            optname = opt[0]
            optvalues[optname] = self.env[optname]
        return optvalues

    def build(self):
        buildable = [feature for feature in self if not feature.skipbuild]
        for feature in buildable:
            feature.build(self.env, self.defs)

def exists(env):
    """Whether this tool exists or not. Implicitely called by scons
    when the tool is loaded.
    """
    return True

def generate(env):
    """Adds the gnomebuild stuff to the construction environment
    'env'. Implicitely called by scons when the tool is loaded.
    """
    global ENV
    ENV = env

    # Add environment variables. I don't understand why this has to be
    # done both here and in the constructor call.
    for k in os.environ.keys():
        ENV[k] = os.environ[k]
    
    setupOptions()
    addInstallPaths()

    ENV.defs = settings.defines()
    ENV.features = featuremanager(ENV, ENV.defs)

    # Add an object holding user defined CCFLAGS
    ENV.CCFLAGS = cflagslist()

    # Return useful utility function
    ENV.DoHelp = DoHelp

