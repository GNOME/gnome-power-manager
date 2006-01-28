from subprocess import Popen, PIPE

class version:
    def __init__(self, major = 0, minor = 0, micro = 0):
        self.major = major
        self.minor = minor
        self.micro = micro

    def __str__(self):
        return "%s.%s.%s" % (self.major, self.minor, self.micro)

    def sum(self):
        return self.major * 10000 + self.minor * 100 + self.micro

    def __eq__(self, other):
        return self.sum() == other.sum()

    def __ge__(self, other):
        return self.sum() >= other.sum()

    def __lt__(self, other):
        return self.sum() < other.sum()


    @classmethod
    def fromstring(cls, str):
        # Only the three first values are significant. We ignore the
        # rest.
        return version(*(int(part) for part in str.split(".")[:3]))

    @classmethod
    def frompkgmodule(cls, pkgmodule):
        """You must ensure that pkg-config is availible and in the
        path before using this. Otherwise, suffer an exception!
        """
        args = ["pkg-config", "--modversion", pkgmodule]
        pkgproc = Popen(args, stderr = PIPE, stdout = PIPE)
        pkgproc.wait()
        if pkgproc.returncode != 0:
            return None
        return version.fromstring(pkgproc.stdout.read())

if __name__ == "__main__":
    assert str(version(3, 2, 1)) == "3.2.1"
    assert isinstance(version.fromstring("1").major, int)
    assert version.fromstring("3.2") == version(3, 2)
    assert version(3, 2) >= version(1, 0, 3)
    assert version.frompkgmodule("foobar") == None
    assert version.frompkgmodule("glib-2.0") == version(2, 8, 3)
    assert version(0, 2, 1) < version(0, 9)
    print "OK!"
