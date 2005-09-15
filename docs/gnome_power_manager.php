<?php
	include('./header.php');
	include('./menu.php');
?>
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

<h2>Notification Icon</h2>
<p>
The notification icon can display a device in the tray.
The icons can be themed with custom icons for each theme, or fallback to a standard default.
</p>

<center>
<img src="images/gpm-taskbar.png" alt="[img]"/>
</center>
<p class="caption">
Example right-click menu that gives the options and actions
</p>

<h2>
<a name="uses"></a>Uses of GNOME Power Manager infrastructure</h2>
<ul>
<li>A dialogue that warns the user when on UPS power, that automatically begins a kind shutdown when the power gets critically low.</li>
<li>An icon that allows a user to dim the LCD screen with a slider, and does do automatically when going from mains to battery power on a laptop.</li>
<li>An icon, that when an additional battery is inserted, updates it's display to show two batteries and recalculates how much time remaining. Would work for wireless mouse and keyboards, UPS's and PDA's.</li>
<li>A daemon that does a clean shutdown when the battery is critically low or does a soft-suspend when you close the lid on your laptop (or press the "suspend" button on your PC).</li>
<li>Tell Totem to use a codec that does low quality processing to conserve battery power.</li>
<li>Postpone indexing of databases (e.g. up2date) or other heavy operations until on mains power.</li>
<li>Presentation programs / movie players don't want the screensaver starting or screen blanking.</li>
</ul>

<?php
	include('./footer.php');
?>
