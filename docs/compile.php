<?php
	include('./header.php');
	include('./menu.php');
?>

<h2>Precompiled Files</h2>
<table>
 <tr>
  <td><img src="images/icon-redhat.png"/></td>
  <td>Package by me for <a href="https://sourceforge.net/project/showfiles.php?group_id=133929">Fedora Core Rawhide</a>
 </tr>
 <tr>
  <td><img src="images/icon-ubuntu.png"/></td>
  <td>Packages by Oliver Grawert for <a href="http://packages.ubuntu.com/breezy/gnome/gnome-power">Ubuntu Breezy</a>
 </tr>
 <tr>
  <td><img src="images/icon-forsight.png"/></td>
  <td>Package by Ken Vandine for <a href="http://www.foresightlinux.com/downloads/">Forsight Linux</a>
 </tr>
 <tr>
  <td><img src="images/icon-gentoo.png"/></td>
  <td>ebuild by Steev Klimaszewski for <a href="https://dev.cardoe.com/gentopia">Gentoo Linux</a>
 </tr>
</table>

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
You can get project statistics from <a href="http://cia.navi.cx/stats/project/gnome/gnome-power-manager/.rss">navi.cx</a> in RSS format or access graphs from <a href="http://sourceforge.net/project/stats/?group_id=133929&ugn=gnome-power">Sourceforge</a>/
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
./autogen.sh ./configure --prefix=~/root --with-gconf-source=xml::/home/hughsie/.gconf --enable-libnotify
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
