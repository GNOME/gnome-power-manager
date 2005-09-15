<?php
	include('./header.php');
	include('./menu.php');
?>

<h2>Why is this important</h2>

<p>
<b>Power management in Linux sucks.</b>
</p>
<p>
Depending if you are running a PPC or i386 PC the different power management facilities are vastly different.
To get your machine to suspend on lid press is already possible, but is difficult to know what config files to modify.
To get your LCD screen brightness set to 50% when you remove the AC Adapter of your laptop is probably possible with a clever little Perl script, but is not something that comes ready configured on a standard Linux distro.
Any of these things need the user to become the super-user to do the action.
</p>
<p>
This needs to change before Linux is accepted as a contender for the corporate desktop.
</p>

<h2>Introduction</h2>
<p>
Power management is an essential job on portable computers, and becoming more important on todays high-powered desktops.
It uses many complex (and sometimes experimental) parts of the system - each of which are slightly different, and may contain errata to work around.
The power management policy could be influenced and tweaked by an huge number of options, and each new laptop model brings more possibilities and options.
Nevertheless it should work in the background <b>without even noticed by the user</b>.
</p>

<p>
For example modern machines allow a many options to reduce power consumption:
</p>
<ul>
<li>Reduce CPU frequency</li>
<li>Switch off the harddrive if not needed</li>
<li>Dim the display when the machine is idle</li>
<li>Tweak the memory management to allow longer cache-intervals for the hard drive</li>
</ul>
<p>
And they support various actions to match users needs:
</p>
<ul>
<li>Suspend to RAM</li>
<li>Suspend to Disk (Hibernate)</li>
<li>Shutdown</li>
<li>Blank screen</li>
</ul>

<?php
	include('./footer.php');
?>
