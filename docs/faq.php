<?php
	include('./header.php');
	include('./menu.php');
?>

<h2>Frequently Asked Questions</h2>
<p>
 Please read through these questions before asking the list these common questions.
</p>

<hr>
<p class="faqquestion">
I run Debian stable/unstable. Can I run GNOME Power Manager?
</p>
<p class="faqanswer">
No. Debian do not follow new HAL and DBUS releases like other bleeding-edge distros do.
Ubuntu is the only Debian based distro that I know that follows upstream so closely.
</p>

<hr>
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

<hr>
<p class="faqquestion">
Why do we need such an up-to-date HAL?
</p>
<p class="faqanswer">
G-P-M uses advanced features of HAL that <b>did not exist</b> in past versions.
You cannot patch G-P-M to support an older HAL, as it *will not* work.
Encourage your distro to update their version of HAL, and then G-P-M will work.
</p>

<hr>
<p class="faqquestion">
Why doesn't my video adaptor come back after a suspend? It just displays black.
</p>
<p class="faqanswer">
You may need to add a s3 command to your kernel boot string so that the kernel can re-initialise your video card.
See <a href="data/video.txt">this document</a> for more details, or to see if your system has been identified as needing the switch.
</p>

<?php
	include('./footer.php');
?>
