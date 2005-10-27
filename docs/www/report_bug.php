<?php
	include('./header.php');
	include('./menu.php');
?>

<h1>Reporting a Bug</h2>
<h2>
Find an existing bug if possible
</h2>
<p>
 First, please find the 
 <a href="http://bugzilla.gnome.org/buglist.cgi?product=gnome-power-manager">
 list of bugs</a> so that you are sure the bug you are reporting isn't a 
 duplicate bug that someone else has fixed.
</p>
<p>
 If not present, then please 
 <a href="http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-power-manager">
 create a new bug</a> attaching *all* the information to this new bug.
</p>
<p>
 Please <b>do not</b> email me bug reorts, as the volume of email I get is
 already huge. Bugzilla lets me keep it all the information in one place,
 so I don't loose track!
</p>
<h2>
<a name="general_bugs"></a>General Bugs
</h2>
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
gnome-power-manager --no-daemon --verbose
</pre>

Then <a href="mailto:richard@hughsie.com">email me</a> as much of this data as possible.

<h2>
<a name="suspend_does_nothing"></a>Hibernate and Suspend do *nothing*
</h2>
When you try to suspend or hibernate, HAL executes the /usr/sbin/hal-system-power-{hibernate|suspend} scripts.
Try running:
<pre>/usr/sbin/hald --daemon=no --verbose=yes --retain-privileges</pre>
and start 
<pre>
gnome-power-manager --verbose
</pre>
in another tab. Then try to suspend.
<?php
	include('./footer.php');
?>
