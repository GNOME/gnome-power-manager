<?php
	include('./header.php');
	include('./menu.php');
?>

<h2>Reporting a Bug</h2>
<p>
 When GNOME Power Manager doesn't work, there is lots that could be wrong.
 To help me diagnose the problem, please supply the following information:
</p>
<ul>
<li>The version of GNOME Power Manager installed.</li>
<li>The version of DBUS installed.</li>
<li>The version of HAL installed.</li>
<li>The kernel version installed.</li>
<li>The raw ACPI data supplied by ACPI (if applicable).</li>
<li>The installation method you used (i.e. FC4 RPM, tarball).</li>
<li>Your distribution and version.</li>
<li>The verbose trace from gnome-power-manager</li>
<li>Any other information that may be relevant.</li>
</ul>

<p>You can use these commands to get all this data quickly.</p>
<pre>
dbus-binding-tool --version | grep Tool
hal-get-property --version
gnome-power-manager --version
uname -r
</pre>

<p>Do this as the root user.</p>
<pre>
for i in /proc/acpi/battery/*/*; do echo $i; cat $i; done
</pre>

<p>This will turn on verbose debugging for GNOME Power Manager.</p>
<pre>
gnome-power-manager --verbose
</pre>

Then <a href="mailto:richard@hughsie.com">email me</a> as much of this data as possible.

<?php
	include('./footer.php');
?>
