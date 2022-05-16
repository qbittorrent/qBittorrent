# How to contribute to qBittorrent

There are three main ways to contribute to the project.
Read the respective section to find out more.

### Table Of Contents

*   **[Bug reporting etiquette](#bug-reporting-etiquette)**
*   **[Submitting an issue/bug report](#submitting-an-issuebug-report)**
    *   [What is an actual bug report?](#what-is-an-actual-bug-report)
    *   [Before submitting a bug report](#before-submitting-a-bug-report)
    *   [Steps to ensure a good bug report](#steps-to-ensure-a-good-bug-report)
*   **[Suggesting enhancements/feature requests](#suggesting-enhancementsfeature-requests)**
    *   [Before submitting an enhancement/feature request](#before-submitting-an-enhancementfeature-request)
    *   [Steps to ensure a good enhancement/feature suggestion](#steps-to-ensure-a-good-enhancementfeature-suggestion)
*   **[Opening a pull request](#opening-a-pull-request)**

# Bug reporting etiquette

*   Issues, pull requests, and comments must always be in **English.**

*   This project is supported by volunteers, do not expect "customer support"-style interaction.

*   **Be patient.** The development team is small and resource limited. Developers and contributors take from their free time to analyze the problem and fix the issue. :clock3:

*   Harsh words or threats won't help your situation. Your complaint will (very likely) be **ignored.** :fearful:

# Submitting an issue/bug report

This section guides you through submitting an issue/bug report for qBittorrent.

Following these guidelines helps maintainers and the community understand your report, reproduce the behavior, and find related reports.

Make sure to follow these rules carefully when submitting a bug report. Failure to do so will result in the issue being closed.

## What is an actual bug report?

Developers and contributors are not supposed to deal with issues for which little to no investigation to find the actual cause of a purported issue was made by the reporter.

Positive contributions are those which are reported with efforts to find the actual cause of an issue, or at the very least efforts were made to narrow it as much as possible.

Requiring people to investigate as much as possible before opening an issue avoids burdening the project with invalid issues or issues unrelated to qBittorrent.

The following are _not_ bug reports. **Check the [wiki][wiki-url], [forum][forum-url] or other places for help and support for issues like these**:

-   Explanation of qBittorrent options (see [wiki][wiki-url]).
-   Help with WebUI setup.
-   Help with embedded tracker setup.
-   Help about BitTorrent in general.
-   Issues with specific search plugins.
-   Asking for specific builds of qBittorrent other than the current one. You can install older releases at your own risk or for regression testing purposes. Previous Windows and macOS builds are available [here][builds-url].
    -   If you want older Linux builds, you will have to compile them yourself from the corresponding commits, or ask someone on the [forum][forum-url] to do it for you.
-   Possibly others. Read on and use common sense.

The issue tracker is for provable issues only: You will have to make the case that the issue is really with qBittorrent and not something else on your side.

To make a case means to provide detailed steps so that anybody can reproduce the issue.
Be sure to rule out that the issue is not caused by something specific on your side.

Issue reports for bugs that apparently aren't easily reproducible or that you can't figure out what triggers it even though you tried are OK.

Any issue opened without effort to provide the required details for developers, contributors or anybody else to reproduce the problem will be closed as invalid.
For example:
-   Crash reports with just a stack trace.
-   Speculated performance issues that do not come with actual profiling data + analysis supporting the claim.

## Before submitting a bug report

-   **Do some basic troubleshooting (examples)**:
    -   Restart qBittorrent.
    -   Restart your PC.
    -   Update your OS (e.g. Windows updates).
    -   Update your network card drivers.
    -   Fully reinstall qBittorrent.
    -   etc...
-   Make sure the problem is not caused by anti-virus or other program messing with your files.
-   Check if you can reproduce the problem in the latest version of qBittorrent.
-   **Check [forum][forum-url] and [wiki][wiki-url].** You might be able to find the cause of the problem and fix things yourself.
-   **Check if the issue exists already in the issue tracker.**
    -   If it does and the issue is still open, add a comment to the existing issue instead of opening a new one.
    -   If you find a Closed issue that seems like it is the same thing that you're experiencing, open a new issue and include a link to the original issue in the body of your new one.
-   If the issue is with the search functionality:
    -   **Make sure you have [`python`][python-url] installed correctly (remember the search functionality requires a working python installation).**
    -   Make sure it is in fact a problem with the search functionality itself, and not a problem with the plugins. If something does not work properly with the search functionality, the first step is to rule out search plugin-related issues.
        -   For search plugin issues, report on the respective search plugin support page, or at [qbittorrent/search-plugins][search-plugins-url].

## Steps to ensure a good bug report

**Follow these guidelines** in order to provide as much useful information as possible right away. Not all of them are applicable to all issues, but you are expected to follow most of these steps (use common sense).
Otherwise, we've noticed that a lot of your time (and the developers') gets thrown away on exchanging back and forth to get this information.

*   Use a **clear and descriptive title** for the issue to identify the problem.

*   Post only **one specific issue per submission.**

*   **Fill out the issue template properly.**

-   **Make sure you are using qBittorrent on a supported platform.** Do not submit issues which can only be reproduced on beta/unsupported releases of supported operating systems (e.g. Windows 10 Insider, Ubuntu 12.04 LTS, etc).
These are unstable/unsupported platforms, and in all likelihood, whatever the issue is, it is not related to qBittorrent.

*   **Specify the OS you're using, its version and architecture.**
    *   Examples: Windows 8.1 32-bit, Linux Mint 17.1 64-bit, Windows 10 Fall creators Update 64-bit, etc.


*   **Report only if you run into the issue with an official stable release, a beta release, or with the most recent upstream changes (in this last case specify the specific commit you are on).** (beta testing is encouraged :smile:). We do not provide support for bugs on unofficial Windows builds.

*   **Specify the version of qBittorrent** you are using, as well as its **architecture** (x86 or x64) and its **libraries' versions** (Help -> About -> Libraries).

*   Specify **how you installed**:
    -   Linux: either from the PPA, your distribution's repositories, or compiled from source, or even possibly third-party repositories.
    -   Windows: either from the installer, or compiled from source, or even possibly third-party repositories.
    -   macOS: either from the installer, or compiled from source, or even possibly third-party repositories.


*   **Describe the exact steps which reproduce the problem in as many details as possible.**
    -   For example, start by explaining how you started qBittorrent, e.g. was it via the terminal? Desktop icon? Did you start it as root or normal user?
    -   **When listing steps, don't just say what you did, but explain how you did it.**
        -   For example, if you added a torrent for download, did you do so via a `.torrent` file or via a magnet link? If it was with a torrent file did you do so by dragging the torrent file from the file manager to the transfer list, or did you use the "Add Torrent File" in the Top Bar?
    -   Describe the behavior you observed after following the steps and point out what exactly is the problem with that behavior; this is what we'll be looking for after executing the steps.


*   **Explain which behavior you expected to see instead** and why.

*   Use **screenshots/animated GIFs to help describe the issue** whenever appropriate [(How?)][attachments-howto-url].

*   If the problem wasn't triggered by a specific action, describe what you were doing before the problem happened.

*   **If you are reporting that qBittorrent crashes**, include the stack trace in the report; include it in a code block, a file attachment, or put it in a gist and provide link to that gist.

*   **For performance-related issues**, include as much profiling data as you can (resource usage graphs, etc).

*   Paste the **qBittorrent log** (or put the contents of the log in a gist and provide a link to the gist). The log can be viewed in the GUI (View -> Log -> tick all boxes). If you can't do that, the file is at:
    -   Linux: `~/.local/share/qBittorrent/logs/qBittorrent.log`
    -   Windows: `%LocalAppData%\qBittorrent\logs`
    -   macOS: `~/Library/Application Support/qBittorrent/qBittorrent.log`


*   **Do NOT post comments like "+1" or "me too!"** without providing new relevant info on the issue. Using the built-in reactions is OK though. Remember that you can use the "subscribe" button to receive notifications of that report without having to comment first.

*   If there seems to be an **issue with specific torrent files/magnet links**:
    -   Don't post private `.torrent` files/magnet links, or ones that point to copyrighted content. If you are willing, offer to email a link or the `.torrent` file itself to whoever developer is debugging it and requests it.
    -   Make sure you can't reproduce the problem with another client, to rule out the possibility that the issue is with the `.torrent` file/magnet link itself.


*   A screenshot, transcription or file upload of any of **qBittorrent's preferences that differ from the defaults.** Please include everything different from the defaults whether or not it seems relevant to your issue.

*   **Attachment rules**:
    -   Short logs and error messages can be pasted as quotes/code whenever small enough; otherwise make a gist with the contents and post the link to the gist.
    -   Avoid linking/attaching impractical file formats such as PDFs/Word documents with images. If you want to post an image, just post the image.

### Provide more context by answering these questions (if applicable):

-   Can you **reliably reproduce the issue?** If not, provide details about how often the problem happens and under which conditions it normally happens (e.g. only happens with extremely large torrents/only happens after qBittorrent is open for more than 2 days/etc...)

-   Did the problem start happening recently (e.g. after updating to a new version of qBittorrent) or was this always a problem?

-   If the problem started happening recently, can you reproduce the problem in an older version of qBittorrent?

-   Are you saving files locally (in a disk in your machine), or are you saving them remotely (e.g. network drives)?

-   Are you using qBittorrent with multiple monitors? If so, can you reproduce the problem when you use a single monitor?

Good read: [How to Report Bugs Effectively][howto-report-bugs-url]

# Suggesting enhancements/feature requests

This section guides you through submitting an enhancement suggestion for qBittorrent, including completely new features and minor improvements to existing functionality.

Following these guidelines helps maintainers and the community understand your suggestion and find related suggestions.

## Before submitting an enhancement/feature request

*   Check the [wiki][wiki-url] and [forum][forum-url] for tips — you might discover that the enhancement is already available.
*   Most importantly, check if you're using the latest version of qBittorrent and if you can get the desired behavior by changing qBittorrent's settings.
*   Check in the [releases][releases-url] page or on the [forum][forum-url], see if there's already a alpha/beta version with that enhancement.
*   Perform a cursory search to see if the enhancement has already been suggested. If it has, add a comment to the existing issue instead of opening a new one.

## Steps to ensure a good enhancement/feature suggestion

-   Specify which version of qBittorrent you're using.
-   Specify the name and version of the OS you're using.
-   Provide a step-by-step description of the suggested enhancement in as many details as possible.
-   Describe the current behavior and explain which behavior you expected to see instead and why.
-   Include screenshots and animated GIFs which help you demonstrate the steps or point out the part of qBittorrent which the suggestion is related to.
-   If this enhancement exists in other BitTorrent clients, list those clients.

# Opening a pull request

*   Consult [coding guidelines][coding-guidelines-url] first. If you are working on translation/i18n, read ["How to translate qBittorrent"][how-to-translate-url].
*   Keep your git commit history clean.
    * Refer to the section about ["Git commit messages"][coding-guidelines-git-commit-message-url] in the coding guidelines.
    * When merge conflicts arise, do `git rebase <target_branch_name>` and fix the conflicts, don't do `git pull`. Here is a good explanation: [merging-vs-rebasing][merging-vs-rebasing-url].
*   Keep pull request title concise and provide motivation and "what it does" in the pull request description area. Make it easy to read and understand.
*   Provide screenshots for UI related changes.
*   If your commit addresses a reported issue (for example issue #8454), append the following text to the commit body `Closes #8454.`. Example [commit][commit-message-fix-issue-example-url].
*   Search [pull request list][pull-request-list-url] first. Others might have already implemented your idea (or got rejected already).

[attachments-howto-url]: https://help.github.com/articles/file-attachments-on-issues-and-pull-requests
[builds-url]: https://sourceforge.net/projects/qbittorrent/files/
[coding-guidelines-url]: https://github.com/qbittorrent/qBittorrent/blob/master/CODING_GUIDELINES.md
[coding-guidelines-git-commit-message-url]: https://github.com/qbittorrent/qBittorrent/blob/master/CODING_GUIDELINES.md#10-git-commit-message
[commit-message-fix-issue-example-url]: https://github.com/qbittorrent/qBittorrent/commit/c07cd440cd46345297debb47cb260f8688975f50
[forum-url]: http://forum.qbittorrent.org/
[howto-report-bugs-url]: https://www.chiark.greenend.org.uk/~sgtatham/bugs.html
[how-to-translate-url]: https://github.com/qbittorrent/qBittorrent/wiki/How-to-translate-qBittorrent
[merging-vs-rebasing-url]: https://www.atlassian.com/git/tutorials/merging-vs-rebasing
[pull-request-list-url]: https://github.com/qbittorrent/qBittorrent/pulls
[python-url]: https://www.python.org/
[releases-url]: https://github.com/qbittorrent/qBittorrent/releases
[search-plugins-url]: https://github.com/qbittorrent/search-plugins
[wiki-url]: https://github.com/qbittorrent/qBittorrent/wiki
