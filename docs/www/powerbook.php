<?php
	include('./header.php');
	include('./menu.php');
?>

<h1>What needs to be done for 100% PMU support in HAL</h1>

<p>
 Things that I need to figure out before PMU is as well supported as ACPI
</p>

<h2>How to set the LCD brightness in software.</h2>

<p>
 ACPI uses calls to /proc/acpi/vendor/* to set the brightness, but with PMU we have to do an ioctl on the pmu device.
 We need to add this code to HAL so GNOME Power Manager can find the capability "laptop_panel" and be able to set the brightness.
 I think one way to do this is a C file, compiled in hal/tools/, called hal-pmu-compat that accepts the different command line parameters and prints to stdout, e.g.
</p>
<pre>
hal-pmu-compat set brightness 80
hal-pmu-compat get brightness
</pre>
<p>
 The other way to this would be to write a library file to be included in HAL for the other functionality, which could then be used for this tool too.
 This would let us set the other stuff from PMU just using addons and probers.
</p>

<h2>How to get the "Ambient light level sensor" into HAL.</h2>

<p>
 We need to be able to get the value of this sensor, so we can set the LCD brightness according to ambient light level.
</p>

<p>
 If you can help me with any of the above, have a good URL, or want to do some coding, then please <a href="mailto:richard@hughsie.com">email me</a>.
 I have not the funds to buy a second-hand powerbook to do the development on, and so the PMU support in HAL (and consequently GNOME Power Manager) will always be less than ACPI.
 If anyone can <i>loan</i> or give me an old powerbook to do the development then PMU should get as much attention as ACPI has right now.
</p>

</p>
 Thanks, Richard.
</p>
<?php
	include('./footer.php');
?>
