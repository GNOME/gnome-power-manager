<?php
	include('./header.php');
	include('./menu.php');
?>

<h2>GNOME Power Preferences</h2>

<p>
Power preferences is a simple program that lets the authorized user change the actions associated with each action. 
Devices that are not present are not shown, e.g. the UPS options are not visible to the user, unless one is plugged in, similarly, battery options are hidden for desktop users.
</p>

<center>
<img src="images/pref-main.png" alt="[img]"/>
</center>
<p class="caption">
Main preferences sliders
</p>

<center>
<img src="images/pref-options.png" alt="[img]"/>
</center>
<p class="caption">
Preferences options
</p>

<center>
<img src="images/pref-advanced.png" alt="[img]"/>
</center>
<p class="caption">
Advanced preferences
</p>

<?php
	include('footer.php');
?>
