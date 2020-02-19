# How to contribute to qBittorrent

There are three main ways to contribute to the project.
Read the respective section to find out more.

### Table Of Contents

- **[Bug reporting etiquette](#bug-reporting-etiquette)**

- **[Submitting an issue/bug report](#submitting-an-issuebug-report)**
  - [What is an actual bug report?](#what-is-an-actual-bug-report)
  - [Before submitting a bug report](#before-submitting-a-bug-report)
  - [Steps to ensure a good bug report](#steps-to-ensure-a-good-bug-report)
    - [1 - Basics](#1---basics)
    - [2 - Software versions](#2---software-versions)
    - [3 - Issue description](#3---issue-description)
      - [Additional steps for issues about crashes](#additional-steps-for-issues-about-crashes)
      - [Additional steps for issues about performance](#additional-steps-for-issues-about-performance)
      - [Additional steps for issues about network connectivity](#additional-steps-for-issues-about-network-connectivity)
    - [4 - Link and attachment rules](#4---link-and-attachment-rules)

- **[Suggesting enhancements/feature requests](#suggesting-enhancementsfeature-requests)**
  - [Before submitting an enhancement/feature request](#before-submitting-an-enhancementfeature-request)
  - [Steps to ensure a good enhancement/feature suggestion](#steps-to-ensure-a-good-enhancementfeature-suggestion)

- **[Opening a pull request](#opening-a-pull-request)**
  - [Must read](#must-read)
  - [Good to know](#good-to-know)

# Bug reporting etiquette

- Issues, pull requests, and comments must always be in **English.**

- This project is supported by volunteers, do not expect "customer support"-style interaction.

- **Be patient.** The development team is small and resource limited.
    Developers and contributors take from their free time to analyze the problem and fix the issue. :clock3:

- Harsh words or threats won't help your situation.
    What's worse, your complain will (very likely) be **ignored.** :fearful:

# Submitting an issue/bug report

This section guides you through submitting an issue/bug report for qBittorrent.

Following these guidelines helps maintainers and the community understand your report, reproduce the buggy behavior, fix it, and find related reports.

Make sure to follow these rules carefully when submitting a bug report.
Failure to do so will result in the issue being closed.

## What is an actual bug report?

Developers and contributors are not supposed to deal with issues for which little to no investigation to find the actual cause of a purported issue was made by the reporter.

Positive contributions are those which are reported with efforts to find the actual cause of an issue, or at the very least efforts were made to narrow it as much as possible.

Requiring people to investigate as much as possible before opening an issue will more than likely avoid burdening the project with invalid issues or issues unrelated to qBittorrent.

The following are _not_ bug reports.
**Check the [wiki][wiki-url], [forum][forum-url] or other places for help and support for issues like these**:

- Explanation of qBittorrent options (see [wiki][wiki-url]).
- Help with WebUI setup.
- Help with embedded tracker setup.
- Help about BitTorrent in general.
- Issues with specific search plugins.
- Asking for specific builds of qBittorrent other than the current one.
    You can install older releases at your own risk or for regression testing purposes.
    Previous Windows and macOS builds are available [here][builds-url].
  - If you want older Linux builds, you will have to compile them yourself from the corresponding commits, or ask someone on the [forum][forum-url] to do it for you.
- Possibly others.
    Read on and use common sense.

The issue tracker is for **provable** issues only: You will have to make the case that the issue is really with qBittorrent and not something else on your side.

A good issue report should provide detailed steps so that anybody can **reproduce** the issue.
Issue reports for bugs that apparently aren't easily reproducible or that you can't figure out what triggers it despite your best efforts are OK.

Any issue opened without effort to provide the required details for developers, contributors or anybody else to reproduce the problem will be closed as **invalid**.
For example:

- Crash reports with just a stack trace.
    Read the [Additional steps for issues about crashes](#additional-steps-for-issues-about-crashes) subsection for more info about what you need to do in this case.
- Speculated performance issues that do not come with actual profiling data + analysis supporting the claim.
    Read the [Additional steps for issues about performance](#additional-steps-for-issues-about-performance) subsection for more info about what you need to do in this case.
- The same goes for network connectivity issues.
    Read the [Additional steps for issues about network connectivity](#additional-steps-for-issues-about-network-connectivity) subsection for more information.

## Before submitting a bug report

- **Do some basic troubleshooting (examples)**:
  - Restart qBittorrent.
  - Restart your PC.
  - Fully reinstall qBittorrent.
    - This means removing qBittorrent **and** [resetting all settings to defaults][wiki-settings-reset-url].
    Be sure to backup those files in case you want to restore them later.
  - Update your OS (e.g. Windows updates).
  - Update your network card drivers.
  - etc...
- Make sure the problem is not caused by anti-virus/firewall, gaming/overclocking overlay, or some other program messing with your files.
- Make sure the issue is not related to insufficient file/folder permissions
  - Windows users: normally you should never need to run qBittorrent as Administrator under any circumstances.
    If you need to do so, you have other issues.
- Check if you can reproduce the problem in the **latest** version of qBittorrent.
- **Check [forum][forum-url] and [wiki][wiki-url].** You might be able to find the cause of the problem and fix things yourself.
- **Read through the [FAQ][wiki-faq-url]**
- **Check if the issue exists already in the issue tracker.**
  - If it does and the issue is still open, add a comment to the existing issue instead of opening a new one.
  - If you find a `Closed` issue that seems like it is the same thing that you're experiencing, open a new issue and include a link to the original issue in the body of your new one.
- If the issue is with the search functionality:
  - **Make sure you have [`python`][python-url] installed correctly (remember the search functionality requires a working python installation).**
  - Make sure it is in fact a problem with the search functionality itself, and not a problem with the plugins.
    If something does not work properly with the search functionality, the first step is to rule out search plugin-related issues.
    - For search plugin issues, report on the respective search plugin support page, or at [qbittorrent/search-plugins][search-plugins-url].

## Steps to ensure a good bug report

**Follow these guidelines** in order to provide as much useful information as possible right away.
Not all of them are applicable to all issues, but you are expected to follow most of these steps (use common sense).

The most important thing is to **provide all of the information requested in the issue template**. Otherwise, we've noticed that a lot of your time (and the developers') gets thrown away on exchanging back and forth to get this information.

### 1 - Basics

- Use a **clear and descriptive title** for the issue to identify the problem.
  - Do NOT put "\[Bug\]", "\[Crash\]" or similar in the title.
    The reports will be appropriately tagged by the maintainers.

- Post only **one specific issue per submission.**

- **Fill out the issue template properly.**

- **Do NOT post "bump" comments like "+1" or "me too!"** without providing new relevant info on the issue.
    If you want to "bump" an issue report, use GitHub's built-in comment reactions (such as :thumbsup:).
    Remember that you can use the "subscribe" button to receive notifications of that report without having to comment first.

### 2 - Software versions

- **Make sure you are using qBittorrent on a supported platform.** Do not submit issues which can only be reproduced on beta/unsupported releases of supported or unsupported operating systems (e.g. Windows 10 Insider, Ubuntu 12.04 LTS, Haiku OS, etc).
    These are unstable/unsupported platforms, and as such whatever the issue is, it is probably not related to qBittorrent.
    qBittorrent is only offically supported on currently supported releases of **Linux distributions, Windows and macOS**.

- **Specify the OS you're using, its version and architecture.**
  - Examples: Windows 8.1 32-bit, Linux Mint 17.1 64-bit, Windows 10 Fall creators Update 64-bit, etc.

- **Report only if you run into the issue with an official stable release, a beta release, or with the most recent upstream changes (in this last case specify the specific commit you are on).** (beta testing is encouraged :smile:).
    We do not provide support for bugs on unofficial Windows builds, such as those obtained from sites like PortableApps.

- **Specify the version of qBittorrent** you are using, as well as its **architecture** (x86 or x64) and its **libraries' versions** (Help -> About -> Libraries).

- Specify **how you installed**:
  - Linux: either from the official PPAs ([stable][ppa-stable-url] or [unstable][ppa-unstable-url]), your distribution's repositories, or compiled from source, or even possibly third-party repositories.
  - Windows: either from the installer (and the site you obtained the installer from), or compiled from source, or even possibly third-party repositories.
  - macOS: either from the installer (and the site you obtained the installer from), or compiled from source, or even possibly third-party repositories.

### 3 - Issue description

- **Describe the exact steps which reproduce the problem in as many details as possible.**
  - For example, start by explaining how you started qBittorrent, e.g. was it via the terminal? Desktop icon? Did you start it as root or normal user?
  - **When listing steps, don't just say what you did, but explain how you did it.**
    - For example, if you added a torrent for download, did you do so via a `.torrent` file or via a magnet link? If it was with a torrent file did you do so by dragging the torrent file from the file manager to the transfer list, or did you use the "Add Torrent File" in the Top Bar?
  - Describe the behavior you observed after following the steps and point out what exactly is the problem with that behavior; this is what we'll be looking for after executing the steps.

- **Explain which behavior you expected to see instead** and why.

- If the problem wasn't triggered by a specific action, describe what you were doing before the problem happened.

- Provide additional data/attachments [(how?)][attachments-howto-url] about your setup
  - Make sure to read and follow the [attachment rules](#4---link-and-attachment-rules).
  - Use **screenshots/animated GIFs to help describe the issue** whenever appropriate.
  - Post the **qBittorrent log and preferences**.
    Check the [FAQ][wiki-faq-url] to learn where qBittorrent saves these files.
    - Note: the log can also be viewed in the GUI (`View` -> `Log` -> tick all boxes).

#### Provide more context by answering these questions (if applicable)

- Can you **reliably reproduce the issue?** If not, provide details about how often the problem happens and under which conditions it normally happens (e.g. only happens with extremely large torrents/only happens after qBittorrent is open for more than 2 days/etc...)

- Did the problem start happening recently (e.g. after updating to a new version of qBittorrent) or was this always a problem?

- If the problem started happening recently, can you reproduce the problem in an older version of qBittorrent?

- Are you saving files locally (in a disk in your machine), or are you saving them remotely (e.g. network drives)?

- Are you using qBittorrent with multiple monitors? If so, can you reproduce the problem when you use a single monitor?

#### Additional steps for issues about crashes

- **If you are reporting that qBittorrent crashes**, include the stack trace in the report; include it in a code block, a file attachment, or put it in a gist and provide link to that gist (remember to read the [attachment rules](#4---link-and-attachment-rules)).

- **Do not _just_ include the stacktrace**, follow the remainding guidelines as well.

#### Additional steps for issues about performance

- **For issues about performance/CPU usage/disk usage, include as much profiling data as you can**, such as:
  - Valgrind output files/results;
  - self-made benchmark results with python/bash scripts/etc;
  - screenshots of qBittorrent's 5 minute+ speedgraph;
  - resource usage graphs from dedicated system monitoring software;
    - Windows users: use the Windows Resource Monitor to generate a report.
        Addtional tools that might be useful include the [Sysinternals Process Monitor][process-monitor-url] and other such tools from the Sysinternals suite.

- **Isolated screenshots of Task Manager or equivalent are not considered sufficient data** to back up a performance issue claim.
    However, if it is your only option, you can post a video/gif showing long-term trends or spikes of a certain metric reported by such a tool.

- **Make sure your testing environment is as controlled as possible and isolated qBittorrent as the cause of the performance issue**.
    For example, qBittorrent could be slow due to something like Chrome using all disk/RAM in the background.

- **If comparing to other clients**, include data (graphs, benchmark results) about their performance as well.
    Don't just say "uTorrent/Deluge/Transmission does X faster", specify in what circumstances and by how much.

- **For reports on bad GUI performance/lag**, a video/gif is always welcome.

- **Examples of good performance issue claims and discussions**:
  - https://github.com/qbittorrent/qBittorrent/issues/12336
    - good textual description, provided relevant details about hardware and attempts to narrow down the problem, and a good screenshot showing long-term trends of the relevant metric.
    Led to the discovery and fix of a bug in `libtorrent`.
  - https://github.com/qbittorrent/qBittorrent/issues/9771
    - good textual description, provided detailed specs, speculated about possible hardware bottleneck, provided screenshot and video showing the described speed trend.
  - https://github.com/qbittorrent/qBittorrent/issues/10309
    - good textual description, quickly led to fix.
  - https://github.com/qbittorrent/qBittorrent/issues/10999
    - good textual description, suggested possible solution, provided a python benchmark script and results in graphs.
  - https://github.com/qbittorrent/qBittorrent/issues/11039
    - good textual description, reasoned about possible causes.
  - https://github.com/qbittorrent/qBittorrent/issues/11822
    - good textual description, but would have benefited from an illustrative video/gif.

#### Additional steps for issues about network connectivity

- **Include as much detail as possible about your environment/network configuration** in your post, for example:
  - Whether or not you're using proxies/VPNs
  - Configuration details about proxies/VPNs, if any
  - Network configuration details of your machine in general
  - Wireshark captures of the relevant traffic or equivalent, if you know how to do them

- **Make sure the issue is not due to a network misconfiguration on your end**.
    For example, ensure Windows Firewall is not blocking outgoing or incoming connections for qBittorrent.

- **Examples of good network connectivity issue claims and discussions**:
  - https://github.com/qbittorrent/qBittorrent/issues/12253
    - good textual description, good attempts to narrow down the problem, provided relevant information throughout the discussion.
  - https://github.com/qbittorrent/qBittorrent/issues/12253
    - good textual description, provided relevant screenshots, experimented with test builds provided by developers, confirming an issue in `Qt`.
  - https://github.com/qbittorrent/qBittorrent/issues/12421
    - good textual descripion, debugged the problem and submitted a PR to fix it.

### 4 - Link and attachment rules

- **Attachment sources/external links**
  - Avoid using external linking/cloud storage services and posting links to them.
    Whenever possible, upload the attachment directly to your post.

- **Privacy**:
  - The qBittorrent logs and settings, as an example, may contain stuff like IP addresses, e-mail addresses, WebUI passwords, torrent names, torrent hashes and the like.
    Some or all of these things may be considered sensitive data that you wouldn't want the world to see.
    Make sure to censor/redact any such data.
    You are solely responsible for the information you post.

- **Torrent files/magnet links**:
  - Don't post private `.torrent` files/magnet links, or ones that point to copyrighted content.
    If you are willing, offer to email a link or the `.torrent` file itself to whoever developer is debugging it and requests it.
  - Make sure you can't reproduce the problem with another client, to rule out the possibility that the issue is with the `.torrent` file/magnet link itself.

- **Attachment size**: Short logs and error messages can be pasted as quotes/code whenever small enough; otherwise make a gist with the contents and post the link to the gist.
    Don't worry about the size of non- plain-text files, such as images and `.zip`s.
    As long as GitHub lets you post them, post them.

- **Attachment formats**:
  - Do not link/attach impractical file formats such as PDFs/Word documents.
    - If you want to post an image, just post the image.
    - If you want to post text, just post the **plain text** file or paste the text directly if it's not too big.
  - Do not post single-file zips, unless the file is to big to be attached unzipped.
    Only zip files together if you have more than 5 attachments in total or if you have less than 5 but they are all related (for example, 3 log files).

Good read: [How to Report Bugs Effectively][howto-report-bugs-url]

# Suggesting enhancements/feature requests

This section guides you through submitting an enhancement suggestion for qBittorrent, including completely new features and minor improvements to existing functionality.

Following these guidelines helps maintainers and the community understand your suggestion and find related suggestions.

## Before submitting an enhancement/feature request

- Check the [wiki][wiki-url] and [forum][forum-url] for tips — you might discover that the enhancement is already available.
- Most importantly, check if you're using the latest version of qBittorrent and if you can get the desired behavior by changing qBittorrent's settings.
- Check in the [releases][releases-url] page or on the [forum][forum-url], see if there's already a alpha/beta version with that enhancement.
- Perform a cursory search to see if the enhancement has already been suggested.
    If it has, add a comment to the existing issue instead of opening a new one.

## Steps to ensure a good enhancement/feature suggestion

- Use a **clear and descriptive title**
  - Do NOT put "\[Feature Request\]", "\[Suggestion\]" or similar in the title.
    The report will be appropriately tagged by the maintainers.
- Specify which version of qBittorrent you're using.
- Specify the name and version of the OS you're using.
- Provide a step-by-step description of the suggested enhancement in as many details as possible.
- Describe the current behavior and explain which behavior you expected to see instead and why.
- Include screenshots and/or animated GIFs which help you demonstrate the steps or point out the part of qBittorrent which the suggestion is related to.
- If this enhancement exists in other BitTorrent clients, list those clients.

# Opening a pull request

### Must read

- Read our [**coding guidelines**][coding-guidelines-url].
    There are some scripts to help you: [uncrustify script][uncrustify-script-url], [astyle script][astyle-script-url], [related thread][coding-guidelines-thread-url].
- Keep the title **short** and provide a **clear** description about what your pull request does.
- Provide **screenshots** for UI related changes.
- Keep your git commit history **clean** and **precise.** Refer to the section about "Git commit messages" in the [**coding guidelines**][coding-guidelines-url].
- If your commit fixes a reported issue (for example #4134), add the following message to the commit `Closes #4134.`.
    Example [here][commit-message-fix-issue-example-url].

### Good to know

- **Search** pull request history! Others might have already implemented your idea and it is waiting to be merged (or got rejected already).
    Save your precious time by doing a search first.
- When resolving merge conflicts, do `git rebase <target_branch_name>`, don't do `git pull`.
    Then you can start fixing the conflicts.
    Here is a good explanation: [link][merging-vs-rebasing-url].

[astyle-script-url]: https://gist.github.com/Chocobo1/539cee860d1eef0acfa6
[attachments-howto-url]: https://help.github.com/articles/file-attachments-on-issues-and-pull-requests
[coding-guidelines-url]: https://github.com/qbittorrent/qBittorrent/blob/master/CODING_GUIDELINES.md
[coding-guidelines-thread-url]: https://github.com/qbittorrent/qBittorrent/issues/2192
[commit-message-fix-issue-example-url]: https://github.com/qbittorrent/qBittorrent/commit/c07cd440cd46345297debb47cb260f8688975f50
[forum-url]: http://forum.qbittorrent.org/
[howto-report-bugs-url]: https://www.chiark.greenend.org.uk/~sgtatham/bugs.html
[merging-vs-rebasing-url]: https://www.atlassian.com/git/tutorials/merging-vs-rebasing
[ppa-stable-url]: https://launchpad.net/~qbittorrent-team/+archive/ubuntu/qbittorrent-stable
[ppa-unstable-url]: https://launchpad.net/~qbittorrent-team/+archive/ubuntu/qbittorrent-unstable
[process-monitor-url]: https://docs.microsoft.com/en-us/sysinternals/downloads/procmon
[python-url]: https://www.python.org/
[releases-url]: https://github.com/qbittorrent/qBittorrent/releases
[search-plugins-url]: https://github.com/qbittorrent/search-plugins
[uncrustify-script-url]: https://raw.githubusercontent.com/qbittorrent/qBittorrent/master/uncrustify.cfg
[wiki-url]: https://github.com/qbittorrent/qBittorrent/wiki
[wiki-faq-url]: https://github.com/qbittorrent/qBittorrent/wiki/Frequently-Asked-Questions
[wiki-settings-reset-url]: https://github.com/qbittorrent/qBittorrent/wiki/Frequently-Asked-Questions#How_can_I_reset_the_settings_to_default_values
[builds-url]: https://sourceforge.net/projects/qbittorrent/files/
