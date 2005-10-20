<?php
	include('./header.php');
	include('./menu.php');
?>

<h2>Precompiled Files</h2>
<p>
 It is advisable to try the pre-compiled package for your distribution before you try to make GNOME Power Manager from source.
 If you want to add a link to a new distro, please drop me an email with the link.
</p>
<table>
 <tr>
  <td valign="top"><img src="images/icon-redhat.png" alt="[img]"/></td>
  <td>
   <p>
    YUM Repo by me for <a href="data/utopia.repo">Fedora Core 4</a>
   </p>
   <p>
    It is recommended that you use the new (and lightly tested) YUM repo - just copy <a href="data/utopia.repo">this file</a> to <b>/etc/yum.repos.d/</b>:
    And do:
   </p>
<pre>
yum -y install gnome-power-manager
</pre>
   <p>
    Also, if you use evince in FC4, the default FC4 version prints "symbol lookup error: evince: undefined symbol: dbus_g_proxy_invoke" on load, and refuses to work.
    Note that the rawhide evince has the same bug, and nothing has been done yet.
    You can use my experimental evince package (dbus stuff turned off, but otherwise identical to the FC4 one) in the above repo, until the evince guys sort out thier dbus versioning.
   </p>
<pre>
yum -y upgrade evince
</pre>
   <p>
    The rawhide kernel is often more current than the FC4 updates kernel, and has many ACPI bugfixes not present in updates-released.
    To run the rawhide kernel on a normal FC4 install <b>you will have to pull in *lots* of system dependancies</b> and these may break a standard FC4 install.
    To do this, optionally do:
   </p>
<pre>
yum -y --enablerepo=development update kernel
</pre>
   <p>
    Also, the current FC4 updates-release kernel, 2.6.12-1.1447_FC4 is *really* broken for ACPI.
    Either use an older kernel, or use the rawhide one. See the bugzilla <a href="https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=167281">here</a>.
   </p>
  </td>
 </tr>
 <tr>
  <td valign="top"><img src="images/icon-ubuntu.png" alt="[img]"/></td>
  <td>
   <p>
    Packages by Oliver Grawert for <a href="http://packages.ubuntu.com/breezy/gnome/gnome-power-manager">Ubuntu Breezy</a>
   </p>
   <pre>apt-get -y install gnome-power-manager libnotify notification-daemon</pre>
  </td>
 </tr>
 <tr>
  <td valign="top"><img src="images/icon-forsight.png" alt="[img]"/></td>
  <td>
   <p>
    Package by Ken Vandine for <a href="http://www.foresightlinux.com/downloads/">Forsight Linux</a>
   </p>
  </td>
 </tr>
 <tr>
  <td valign="top"><img src="images/icon-suse.png" alt="[img]"/></td>
  <td>
   <p>
    Package by James Ogley for <a href="http://www.novell.com/linux/suse/">SUSE Linux</a>
   </p>
   <p>
    To use this APT repo, please follow the instructions on the <a href="http://usr-local-bin.org/rpms/usr-local-bin.php">usr-local-bin.org</a> website.
    Then you can use the following command:
   </p>
<pre>
apt-get update
apt-get upgrade
apt-get install gnome-power-manager
</pre>
  </td>
 </tr>
 <tr>
  <td valign="top"><img src="images/icon-arch.png" alt="[img]"/></td>
  <td>
   <p>
    Compile notes for <a href="http://archlinux.org/">Arch Linux</a>
   </p>
   <p>
    If you try to compile the tarball with Arch Linux, you may get the error
   </p>
<pre>
libtool: link: cannot find the library `/usr/lib/libgobject-2.0.la'
</pre>
   <p>
    GNOME Power Manager uses pkgconfig (like most open-source projects, but Arch Linux has a bug
    where libnotify.la references other files.
    The .la files are soon to be removed from Arch Linux, and if you manually delete the file libnotify.la,
    the tarball will compile. Bug found by Eugenia Loli-Queru.
   </p>
  </td>
 </tr>
 <tr>
  <td valign="top"><img src="images/icon-gentoo.png" alt="[img]"/></td>
  <td>
   <p>
    ebuild by Steev Klimaszewski for <a href="http://gentopia.gentooexperimental.org/">Gentoo Linux</a>
   </p>
<pre>
emerge subversion
cd /usr/local/
svn co https://gentopia.gentooexperimental.org/svn/overlay/ gentopia
edit /etc/make.conf
set PORTDIR_OVERLAY="/usr/local/gentopia"
# If you already have an overlay then do this
PORTDIR_OVERLAY="/usr/local/portage /usr/local/gentopia"
emerge -av dbus hal pmount gnome-power-manager udev
</pre>
  </td>
 </tr>
</table>
<hr/>

<h2>Getting The Source</h2>
<p>
The latest 'stable' tarballs can be found at <a href="https://sourceforge.net/project/showfiles.php?group_id=133929">sourceforge</a>.
</p>

<h3>CVS</h3>
<p>
You can check out the latest GNOME Power Manager code using CVS. You can use the <a href="http://cvs.gnome.org/viewcvs/gnome-power-manager/">viewcvs</a> interface too.
</p>
<pre>
export CVSROOT=":pserver:anonymous@anoncvs.gnome.org:/cvs/gnome"
cvs -z3 checkout gnome-power-manager
</pre>

<h3>jhbuild</h3>
<p>
If you want to use GNOME CVS, jhbuild can now build GNOME Power Manager using:
</p>
<pre>
jhbuild build gnome-power-manager
</pre>

<h2>Compiling The Code</h2>
<p>
To compile and make, I use the following procedure:
</p>
<pre>
cd gnome-power-manager
./autogen.sh ./configure --prefix=/home/hughsie/root --with-gconf-source=xml::/home/hughsie/.gconf --enable-libnotify
make
make install
su
cp ~/root/etc/dbus-1/system.d/gnome-power-manager.conf /etc/dbus-1/system.d/
/sbin/service messagebus restart
exit
</pre>
<p>
NB: Your command for restarting dbus maybe different to the above.
It may be easier to reboot your computer instead of restarting messagebus.
</p>

<hr/>
<h3>Fedora Core 4 &amp; GNOME Power Manager CVS HOWTO</h3>
<p>
<b>THESE NOTES ARE UNFINISHED AND MAY NOT WORK</b>
</p>
<p>
Install DBUS and HAL from Rawhide, and install libnotify, libnotify-devel and notification-daemon as detailed at the top of the page.
You will have to also install standard GNOME build tools (if not already installed), and the other GNU automake type stuff.
Start by opening a terminal and doing (as root):
</p>
   <p>
    You can also install the rawhide versions of DBUS and HAL to satisfy the build requirements.
    To run the rawhide DBUS and HAL on a normal FC4 install <b>you will have to pull in lots of dependancies</b> and lots of stuff <b>may not work</b> normally.
    <b>Big flashing red disclaimers apply. Do not do this on a production machine.</b>
   </p>
<pre>
yum -y --enablerepo=development update hal dbus
yum -y --enablerepo=development install pm-utils
</pre>
<pre>
# yum -y groupinstall "GNOME Software Development"
# yum -y groupinstall "Development Tools"
# yum -y install libwnck-devel
# yum -y install gtk+-devel
# yum -y --enablerepo=development install hal-devel
</pre>
<p>
Now we will build gnome power manager (as a user account).
</p>
<pre>
$ cd gnome-power-manager
$ ./autogen.sh --prefix=/usr/local \
--enable-libnotify=auto \
--with-gconf-source=xml::/etc/gconf/gconf.xml.defaults \
--with-dbus-sys=/etc/dbus-1/system.d \
--with-dbus-services=/usr/share/dbus-1/services
$ make
</pre>
<p>
We are installing two files locally, the DBUS services and system.d files.
We have to install locally so that messagebus picks up our .conf file to allow connections.
Now, as root, we will install the programs, pixmaps and manpages into the /usr/local root.
</p>
<pre>
# make install
# /sbin/service messagebus restart
</pre>
<p>
You should not have seen any arrors in the install. If you do, mail me and I'll update this HOWTO.
Now we can run the compiled program!
</p>
<pre>
$ /usr/local/bin/gnome-power-manager --verbose
</pre>

<?php
	include('./footer.php');
?>
