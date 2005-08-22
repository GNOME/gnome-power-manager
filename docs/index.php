<?php
	include('./header.php');
	include('./menu.php');
?>

<h2>Why is this important</h2>

<p>
<b>Power management in Linux sucks.</b>
Depending if you are running a PPC or i386 PC the different power management facilities are vastly different.
To get your machine to suspend on lid press is already possible, but is difficult to know what config files to modify.
To get your LCD screen brightness set to 50% when you remove the AC Adapter of your laptop is probably possible with a clever little Perl script, but is not something that comes ready configured on a standard Linux distro.
Any of these things need the user to become the super-user to do the action.
</p>
<p>
This needs to change before Linux is accepted as a contender for the corporate desktop.
</p>

<p>
GNOME Power Manager owns the session D-BUS service net.sf.GnomePower and runs a session daemon (i.e. once per logged in user) and optionally displays battery status and low battery notifications. 
</p>

<p>
The session daemon is very resource friendly. Other than the initial coldplug, it uses internal caching for all the power devices, so no additional lookups are needed for each update event.
It will only update the displayed icon on a powerState change, but will update the tooltip on every percentage change.
It should use *very little* CPU and memory.
GNOME Power Manager is written in C, and has additional dependencies of:
</p>
<ol>
<li>hal (0.5.4 or better)</li>
<li>dbus-glib (0.35.2)</li>
<li>libnotify (0.2.1) [optional, but highly recommended]</li>
<li>notification-daemon (0.2.1) [optional, but highly recommended]</li>
</ol>

<?php
	include('./footer.php');
?>
