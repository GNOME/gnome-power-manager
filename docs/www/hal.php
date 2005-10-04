<?php
	include('./header.php');
	include('./menu.php');
?>

<p>
GNOME Power Manager gets all information from HAL using information from org.freedesktop.Hal.
GNOME Power Manager does not do independent probing for data, it relies on HAL, in this way it can stay very lightweight and uncomplicated.
Its goal is to be architecture neutral and free of polling and other hacks.
</p>

<center>
<img src="images/plan.png" alt="[img]"/>
</center>
<p class="caption">
The role GNOME Power Manager, HAL, and the Kernel play in Power Management.
</p>

<p>
So far, we have supported:
</p>

<ol>
<li>Laptop batteries</li>
<li>AC Adapters</li>
<li>APC UPS's</li>
<li>SynCE PDA's</li>
<li>Logitech Wireless Mice</li>
<li>Logitech Wireless Keyboards</li>
</ol>

<h2>How does HAL help?</h2>

<p>
HAL is the de-facto Hardware Abstraction Layer for the Linux desktop.
David Zeuthen (and myself and lots of others) have written different addons for hald (the HAL daemon) that populate different devices with additional properties, e.g. battery.charge.current_level that can be queried in an architecture-neutral way.
</p>
<p>
With all the new HAL code, and the GNOME Power Manager services, we can disable loading of pmud/acpid/apmd and keep everything managed in one place.
</p>
<center>
<img src="images/hal-addbatt.png" alt="[img]"/>
</center>
<p class="caption">
New ACPI objects in the HAL device tree.
</p>

<center>
<img src="images/hal-button.png" alt="[img]"/>
</center>
<p class="caption">
Pressing the power/sleep/lid buttons now generate events
</p>

<center>
<img src="images/hal-suspend.png" alt="[img]"/>
</center>
<p class="caption">
Using the new methods in HAL CVS: org.freedesktop.Hal.Device.SystemPowerManagement.Suspend ()
</p>

<?php
	include('footer.php');
?>
