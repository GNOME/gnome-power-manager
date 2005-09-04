<?php
	include('./header.php');
	include('./menu.php');
?>

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
