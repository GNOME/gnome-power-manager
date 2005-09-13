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
    Package by me for <a href="http://sourceforge.net/project/showfiles.php?group_id=133929">Fedora Core 4</a>
   </p>
   <p>
    You will need to install the rawhide versions of DBUS and HAL to satisfy the build requirements.
    The rawhide kernel is often more current than the FC4 updates kernel, and has many ACPI bugfixes not present in updates-released.
    I have been running the rawhide kernel, DBUS and HAL here on a normal FC4 install and everything apprears to work normally.
    <b>Standard disclaimers apply.</b> Do not do this on a production machine.
   </p>
   <pre>
    yum -y --enablerepo=development update hal dbus kernel
    yum -y --enablerepo=development install pmscripts
   </pre>
   <p>
    You will have to install libnotify and notification daemon to use the libnotify features of GNOME Power Manager.
    libnotify will appear in fedora development soon, but until then you can use <a href="data/">these</a> packages if you wish.
    <b>Standard disclaimers apply.</b>
   </p>
   <pre>rpm -ivh libnotify-0.2.2-1.i386.rpm notification-daemon-0.2.2-1.i386.rpm</pre>
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
  <td valign="top"><img src="images/icon-gentoo.png" alt="[img]"/></td>
  <td>
   <p>
    ebuild by Steev Klimaszewski for <a href="https://dev.cardoe.com/gentopia/">Gentoo Linux</a>
   </p>
<pre>
emerge subversion
cd /usr/local/
svn co https://dev.cardoe.com/gentopia/svn/overlay/ portage-gentopia
edit /etc/make.conf
set PORTDIR_OVERLAY="/usr/local/portage-gentopia"
# If you already have an overlay then do this
# PORTDIR_OVERLAY="/usr/local/portage /usr/local/portage-gentopia"
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
