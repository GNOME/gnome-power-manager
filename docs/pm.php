<?php
	include('./header.php');
	include('./menu.php');
?>

<h2>PowerManager</h2>
<p>
GnomePower uses PowerManager to launch scripts.
<b>PowerManager is a temporary bodge!</b>
When HAL 0.5.4 is released (really soon!) with actions support, PowerManager will become obsolete.
</p>
<p>
PowerManager is a simple DBUS daemon that runs as the root user and holds the system connection name net.sf.PowerManager.
Only root is allowed to run this program, (this can be changed in /etc/dbus-1/system.d/PowerManager.conf) as it's entire purpose is to allow non-root users to run the scripts /usr/sbin/pm-*.
This is also configurable in the above PowerManager.conf config file.
By default, all users are allowed to connect to the system DBUS net.sf.PowerManager connection.
</p>

<pre>
You can issue the following methods:
bool isActive ()
bool shutdown ()
bool restart ()
bool hibernate ()
bool suspend ()
bool hdparm (timeout, device)
</pre>

<p>
You can check out the latest PowerManager code using CVS:
</p>
<pre>
cvs -d:pserver:anonymous@cvs.sf.net:/cvsroot/gnome-power checkout power-manager
</pre>


<?php
	include('./footer.php');
?>
