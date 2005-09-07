<?php
	include('./header.php');
	include('./menu.php');
?>

<h2>Frequently Asked Questions</h2>
<p>
 Please read through these questions before asking the list these common questions.
</p>

<a name="dbus_session_error"></a><hr/>
<p class="faqquestion">
Why do I get the error:
<pre>
./gnome-power-manager(7889): libnotify: Error connecting to session bus: 
Failed to connect to socket /tmp/dbus-Emfs6JOgHK: Connection refused
** ERROR **: Cannot initialise libnotify!
aborting...
</pre>
</p>
<p class="faqanswer">
Most distro's set up the dbus session daemon when you log in to X, or when running GNOME.
If your distro does not do this you will have to manually run:
</p>
<pre>
eval `dbus-launch --auto-syntax`
</pre>
<p class="faqanswer">
If this works, you can add this to your <a href="http://mail.gnome.org/archives/muine-list/2004-December/msg00038.html">X startup</a>.<br>
<b>GNOME Power Manager later than 0.2.1 warns the user, rather than give this cryptic libnotify message</b>
</p>

<hr/>
<p class="faqquestion">
How do I query GNOME Power Manager's battery state from my shell script?
</p>
<p class="faqanswer">
You can use the dbus-send program. For example, to get the boolean value for the IsOnBattery method, you would use the following:
<pre>
dbus-send --session --print-reply \
--dest=org.gnome.GnomePowerManager \
/org/gnome/GnomePowerManager \
org.gnome.GnomePowerManager.isOnBattery
</pre>
</p>

<hr/>
<p class="faqquestion">
Nothing happens when I click suspend or hibernate... What should I do?
</p>
<p class="faqanswer">
HAL might not *yet* support your distro, or you might have found a bug in GNOME Power Manager.
Refer to the <a href="report_bug.php#suspend_does_nothing">Reporting Bugs</a> section.
</p>

<hr/>
<p class="faqquestion">
What is PowerManager - why was GNOME Power Manager depending on it, but now obsoletes it?
</p>
<p class="faqanswer">
Before HAL had the method support (added in 0.5.4), GNOME Power Manager used an external daemon running as root to do all the dirty work.
This daemon was called PowerManager.
This is now obsolete with the newest HAL, and with the new work that we have done, HAL far surpasses the capabilities of PowerManager.
<b>PowerManager and pmscripts are now obsolete.</b>
</p>

<hr/>
<p class="faqquestion">
Can I get updates to the progress of GNOME Power Manager? Do you have an RSS feed?
</p>
<p class="faqanswer">
You can get CVS updates from <a href="http://cia.navi.cx/stats/project/gnome/gnome-power-manager/.rss">navi.cx</a> in RSS format.
There are also project statistics and access graphs provided by <a href="http://sourceforge.net/project/stats/?group_id=133929&amp;ugn=gnome-power">Sourceforge</a>.
</p>

<hr/>
<p class="faqquestion">
I run Debian stable/unstable. Can I run GNOME Power Manager?
</p>
<p class="faqanswer">
No. Debian do not follow new HAL and DBUS releases like other bleeding-edge distros do.
Ubuntu is the only Debian based distro that I know that follows upstream so closely.
</p>

<hr/>
<p class="faqquestion">
Why do we need such an up-to-date DBUS?
</p>
<p class="faqanswer">
The DBUS interface API is changing <b>frequently</b>, and will continue to do so until DBUS 1.0 is released.
Many DBUS apps have lots of configure logic and extra code that I could not maintain - and is a waste of my time to support a version of DBUS that is already obsolete.
I will always track the latest released version.
Also, some bugs in the newer DBUS's have been fixed that impact the operation of GNOME Power Manager.
You are welcome to patch G-P-M to work with an older DBUS if you wish.
</p>

<hr/>
<p class="faqquestion">
Why do we need such an up-to-date HAL?
</p>
<p class="faqanswer">
G-P-M uses advanced features of HAL that <b>did not exist</b> in past versions.
You cannot patch G-P-M to support an older HAL, as it *will not* work.
Encourage your distro to update their version of HAL, and then G-P-M will work.
</p>

<hr/>
<p class="faqquestion">
Why doesn't my video adaptor come back after a suspend? It just displays black.
</p>
<p class="faqanswer">
You may need to add a s3 command to your kernel boot string so that the kernel can re-initialise your video card.
See <a href="data/video.txt">this document</a> for more details, or to see if your system has been identified as needing the switch.
</p>

<hr/>
<p class="faqquestion">
Why doesn't GNOME Power manager autostart?
</p>
<p class="faqanswer">
For the moment, you will have to run GNOME Power Manager from a terminal (it will not autostart).
It will hopefully be either patched into the default upstream startup, or will use gnome-services when it arrives in mainline.
You can also use gnome-session (this is what I do) to launch GNOME Power Manager for each session.
</p>
<center>
<img src="images/gnome-session.png" alt="[img]"/>
</center>
<p class="caption">
You can launch gnome-power-manager automatically using the gnome-session-properties program
</p>

<?php
	include('./footer.php');
?>
