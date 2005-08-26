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
  <td valign="top"><img src="images/icon-redhat.png"/></td>
  <td>
   <p>
    Package by me for <a href="http://sourceforge.net/project/showfiles.php?group_id=133929">Fedora Core 4</a>
   </p>
   <p>
    You will need to install the rawhide versions of DBUS and HAL to satisfy the build requirements.
    I have been running the rawhide DBUS and HAL here on a normal FC4 install and everything apprears to work normally.
    <b>Standard disclaimers apply.</b> Do not do this on a production machine.
   </p>
   <pre>
    yum -y --enablerepo=development update hal dbus
   </pre>
  </td>
 </tr>
 <tr>
  <td valign="top"><img src="images/icon-ubuntu.png"/></td>
  <td>Packages by Oliver Grawert for <a href="http://packages.ubuntu.com/breezy/gnome/gnome-power-manager">Ubuntu Breezy</a></td>
 </tr>
 <tr>
  <td valign="top"><img src="images/icon-forsight.png"/></td>
  <td>Package by Ken Vandine for <a href="http://www.foresightlinux.com/downloads/">Forsight Linux</a></td>
 </tr>
 <tr>
  <td valign="top"><img src="images/icon-gentoo.png"/></td>
  <td>ebuild by Steev Klimaszewski for <a href="http://dev.cardoe.com/gentopia">Gentoo Linux</a></td>
 </tr>
</table>
<hr>

<h2>Getting The Source</h2>
<p>
The latest 'stable' tarballs can be found at <a href="https://sourceforge.net/project/showfiles.php?group_id=133929">sourceforge</a>.
</p>
<p>

<h3>CVS</h3>
<p>
You can check out the latest GNOME Power Manager code using CVS. You can use the <a href="http://cvs.gnome.org/viewcvs/gnome-power-manager/">viewcvs</a> interface too.
</p>
<pre>
export CVSROOT=":pserver:anonymous@anoncvs.gnome.org:/cvs/gnome"
cvs -z3 checkout gnome-power-manager
</pre>

<h3>Statistics</h3>
<p>
You can get project statistics from <a href="http://cia.navi.cx/stats/project/gnome/gnome-power-manager/.rss">navi.cx</a> in RSS format or access graphs from <a href="http://sourceforge.net/project/stats/?group_id=133929&ugn=gnome-power">Sourceforge</a>.
</p>

<h3>jhbuild</h3>
<p>
If you want to use GNOME CVS, jhbuild can now build GNOME Power Manager using:
</p>
<pre>
jhbuild build gnome-power
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

<hr>
<h3>Fedora Core 4 & GNOME Power Manager CVS HOWTO</h3>
<p>
<b>THESE NOTES ARE UNFINISHED AND MAY NOT WORK</b>
</p>
<p>
Install DBUS and HAL from Rawhide as detailed at the top of the page.
We will also install standard GNOME build tools (if not already installed), and the other GNU automake type stuff here too.
Start by opening a terminal and doing (as root):
</p>
<pre>
# yum -y groupinstall "GNOME Software Development"
# yum -y groupinstall "Development Tools"
# yum -y install libwnck-devel
# yum -y install gtk+-devel
# yum -y install hal-devel
</pre>
<p>
We will have to install libnotify and notification daemon from SVN to use the optional libnotify dependancy.
libnotify will appear in fedora development soon, but until then you can use <a href="(TODO)">these</a>  packages if you wish.
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
We have to install locally so that messagebus has to pick up our .conf file to allow connections.
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
<hr>

<h2>Why doesn't it autostart?</h2>
<p>
For the moment, you will have to run GNOME Power Manager from a terminal (it will not autostart.) but will hopefully be either patched into the default upstream startup, or will use gnome-services when it arrives in mainline.
</p>
<center>
<img src="images/gnome-session.png" alt="[img]"/>
</center>
<p class="caption">
You can launch gnome-power-manager automatically using the gnome-session-properties program
</p>

<?php
	include('./footer.php');
?>
