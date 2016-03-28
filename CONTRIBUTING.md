# Filing an issue

### Must read
* If you aren't sure, you can ask on the [**forum**](http://forum.qbittorrent.org) or read our [**wiki**](http://wiki.qbittorrent.org) first.
* Do a quick **search**. Others might already reported the issue.
* Write in **English**!
* Provide **version** information: (You can find version numbers at menu `Help -> About -> Libraries`)
  ```
qBittorrent:
Qt:
libtorrent:
boost:
OS version:

  ```
* Provide **steps** to reproduce the problem, it will be easier to pinpoint the fault.
* **Screenshots**! A screenshot is worth a thousand words. just upload it. [(How?)](https://help.github.com/articles/file-attachments-on-issues-and-pull-requests)

### Good to know
* **Be patient**. The dev team is small and resource limited. Devs finding their free time, analyzing the problem and fixing the issue, it all takes time. :clock3:
* If you can code, why not become a **contributor** by fixing the issue and open a pull request? :wink:
* Harsh words or threats won't help your situation. What's worse, your complain will (very likely) to be **ignored**. :fearful:


# Opening a pull request

### Must read
* Read our [**coding guidelines**](https://github.com/qbittorrent/qBittorrent/blob/master/CODING_GUIDELINES.md). There are some scripts to help you: [uncrustify script](https://raw.githubusercontent.com/qbittorrent/qBittorrent/master/uncrustify.cfg), [astyle script](https://gist.github.com/Chocobo1/539cee860d1eef0acfa6), [(related thread)](https://github.com/qbittorrent/qBittorrent/issues/2192).
* Keep the title **short** and provide a **clear** description about what your pull request does.
* Provide **screenshots** for UI related changes.
* Keep your git commit history **clean** and **precise**. Commits like `xxx fixup` should not appear.
* If your commit fix a reported issue (for example #4134), add the following message to the commit `Closes #4134.`. Example [here](https://github.com/qbittorrent/qBittorrent/commit/a74bac20c4e8de9776bf9bb77fdc7526135d1988).

### Good to know
* **Search** pull request history! Others might already implemented your idea and is waiting to be merged (or got rejected already). Save your precious time by doing a search first.
* When resolving merge conflicts, do `git rebase <target_branch_name>`, don't do `git pull`. Then you can start fixing the conflicts. Here is a good explanation: [link](https://www.atlassian.com/git/tutorials/merging-vs-rebasing).
