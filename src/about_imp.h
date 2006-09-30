/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef ABOUT_H
#define ABOUT_H

#include "ui_about.h"
#define VERSION "v0.7.0rc3"

class about : public QDialog, private Ui::AboutDlg{
  Q_OBJECT

  public:
    about(QWidget *parent = 0): QDialog(parent){
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      // Set icons
      logo->setPixmap(QPixmap(QString::fromUtf8(":/Icons/yinyang32.png")));
      //Title
      lb_name->setText("<b><h1>"+tr("qBittorrent ")+VERSION"</h1></b>");
      // Thanks
      te_thanks->append("<ul><li>I would like to thank sourceforge.net for hosting qBittorrent project.</li>");
      te_thanks->append("<li>I also want to thank Jeffery Fernandez (jeffery@qbittorrent.org), project consultant, webdevelopper and RPM packager, for his help.</li>");
      te_thanks->append("<li>I am gratefull to Peter Koeleman (peter@qbittorrent.org) and Johnny Mast (rave@qbittorrent.org) who helped me port qBittorrent to Windows.</li>");
      te_thanks->append("<li>Thanks a lot to our graphist Mateusz Toboła (tobejodok@qbittorrent.org) for his great work.</li></ul>");
      // Translation
      te_translation->append(tr("I would like to thank the following people who volunteered to translate qBittorrent:")+"<br>");
      te_translation->append(QString::fromUtf8(
          "<i>- <u>Bulgarian:</u> Tsvetan & Boiko Bankov (emerge_life@users.sourceforge.net)<br>\
          - <u>Catalan:</u> Gekko Dam Beer (gekko04@users.sourceforge.net)<br>\
          - <u>Chinese (Simplified):</u> Chen Wuyang (wuyang@gmail.com)<br>\
          - <u>Chinese (Traditional):</u> Jeff Chen (jeff.cn.chen@gmail.com)<br>\
	  - <u>Dutch:</u> Luke Niesink (luke@lukeniesink.net)<br>\
          - <u>German:</u> Niels Hoffmann (zentralmaschine@users.sourceforge.net)<br>\
	  - <u>Greek:</u> Tsvetan Bankov (emerge_life@users.sourceforge.net)<br>\
          - <u>Italian:</u> Maffo (maffo999@users.sourceforge.net)<br>\
          - <u>Korean:</u> Jin Woo Sin (jin828sin@users.sourceforge.net)<br>\
          - <u>Polish:</u> Adam Babol (a-b@users.sourceforge.net)<br>\
          - <u>Portuguese:</u> Bruno Nunes (brunopatriarca@users.sourceforge.net)<br>\
          - <u>Romanian:</u> Obada Denis (obadadenis@users.sourceforge.net)<br>\
          - <u>Russian:</u> Nick Khazov (m2k3d0n at users.sourceforge.net)</i><br>\
          - <u>Slovak:</u>  helix84</i><br>\
          - <u>Spanish:</u> Vicente Raul Plata Fonseca (silverxnt@users.sourceforge.net)</i><br>\
	  - <u>Swedish:</u> Daniel Nylander (po@danielnylander.se)</i><br>\
          - <u>Turkish:</u> Erdem Bingöl (erdem84@gmail.com)</i><br>\
          - <u>Ukrainian:</u> Andrey Shpachenko (masterfix@users.sourceforge.net)</i><br><br>"));
      te_translation->append(tr("Please contact me if you would like to translate qBittorrent to your own language."));
      // License
      te_license->append("<center><b>GNU GENERAL PUBLIC LICENSE</b></center><br>\
          <center>Version 2, June 1991</center><br>\
          Copyright (C) 1989, 1991 Free Software Foundation, Inc.<br>\
          51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA<br>\
          Everyone is permitted to copy and distribute verbatim copies<br>\
          of this license document, but changing it is not allowed.<br>\
          <br>\
          <center><b>Preamble</b></center><br>\
          The licenses for most software are designed to take away your<br>\
          freedom to share and change it.  By contrast, the GNU General Public<br>\
          License is intended to guarantee your freedom to share and change free<br>\
          software--to make sure the software is free for all its users.  This<br>\
          General Public License applies to most of the Free Software<br>\
          Foundation's software and to any other program whose authors commit to<br>\
          using it.  (Some other Free Software Foundation software is covered by<br>\
          the GNU Library General Public License instead.)  You can apply it to<br>\
          your programs, too.<br>\
          <br>\
          When we speak of free software, we are referring to freedom, not<br>\
          price.  Our General Public Licenses are designed to make sure that you<br>\
          have the freedom to distribute copies of free software (and charge for<br>\
          this service if you wish), that you receive source code or can get it<br>\
          if you want it, that you can change the software or use pieces of it<br>\
          in new free programs; and that you know you can do these things.<br>\
          <br>\
          To protect your rights, we need to make restrictions that forbid<br>\
          anyone to deny you these rights or to ask you to surrender the rights.<br>\
          These restrictions translate to certain responsibilities for you if you<br>\
          distribute copies of the software, or if you modify it.<br>\
          <br>\
          For example, if you distribute copies of such a program, whether<br>\
          gratis or for a fee, you must give the recipients all the rights that<br>\
          you have.  You must make sure that they, too, receive or can get the<br>\
          source code.  And you must show them these terms so they know their<br>\
          rights.<br>\
          <br>\
          We protect your rights with two steps: (1) copyright the software, and<br>\
          (2) offer you this license which gives you legal permission to copy,<br>\
          distribute and/or modify the software.<br>\
          <br>\
          Also, for each author's protection and ours, we want to make certain<br>\
          that everyone understands that there is no warranty for this free<br>\
          software.  If the software is modified by someone else and passed on, we<br>\
          want its recipients to know that what they have is not the original, so<br>\
          that any problems introduced by others will not reflect on the original<br>\
          authors' reputations.<br>\
          <br>\
          Finally, any free program is threatened constantly by software<br>\
          patents.  We wish to avoid the danger that redistributors of a free<br>\
          program will individually obtain patent licenses, in effect making the<br>\
          program proprietary.  To prevent this, we have made it clear that any<br>\
          patent must be licensed for everyone's free use or not licensed at all.<br>\
          <br>\
          The precise terms and conditions for copying, distribution and<br>\
          modification follow.<br>\
          <br>\
          <center><b>GNU GENERAL PUBLIC LICENSE</b></center><br>\
          <center><b>TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION</b></center><br>\
          0. This License applies to any program or other work which contains<br>\
          a notice placed by the copyright holder saying it may be distributed<br>\
          under the terms of this General Public License.  The 'Program', below,<br>\
          refers to any such program or work, and a 'work based on the Program'<br>\
          means either the Program or any derivative work under copyright law:<br>\
          that is to say, a work containing the Program or a portion of it,<br>\
          either verbatim or with modifications and/or translated into another<br>\
          language.  (Hereinafter, translation is included without limitation in<br>\
          the term 'modification'.)  Each licensee is addressed as 'you'.<br>\
          <br>\
          Activities other than copying, distribution and modification are not<br>\
          covered by this License; they are outside its scope.  The act of<br>\
          running the Program is not restricted, and the output from the Program<br>\
          is covered only if its contents constitute a work based on the<br>\
          Program (independent of having been made by running the Program).<br>\
          Whether that is true depends on what the Program does.<br>\
          <br>\
          1. You may copy and distribute verbatim copies of the Program's<br>\
          source code as you receive it, in any medium, provided that you<br>\
          conspicuously and appropriately publish on each copy an appropriate<br>\
          copyright notice and disclaimer of warranty; keep intact all the<br>\
          notices that refer to this License and to the absence of any warranty;<br>\
          and give any other recipients of the Program a copy of this License<br>\
          along with the Program.<br>\
          <br>\
          You may charge a fee for the physical act of transferring a copy, and<br>\
          you may at your option offer warranty protection in exchange for a fee.<br>\
          <br>\
          2. You may modify your copy or copies of the Program or any portion<br>\
          of it, thus forming a work based on the Program, and copy and<br>\
          distribute such modifications or work under the terms of Section 1<br>\
          above, provided that you also meet all of these conditions:<br>\
          <br>\
          a) You must cause the modified files to carry prominent notices<br>\
          stating that you changed the files and the date of any change.<br>\
          <br>\
          b) You must cause any work that you distribute or publish, that in<br>\
          whole or in part contains or is derived from the Program or any<br>\
          part thereof, to be licensed as a whole at no charge to all third<br>\
          parties under the terms of this License.<br>\
          <br>\
          c) If the modified program normally reads commands interactively<br>\
          when run, you must cause it, when started running for such<br>\
          interactive use in the most ordinary way, to print or display an<br>\
          announcement including an appropriate copyright notice and a<br>\
          notice that there is no warranty (or else, saying that you provide<br>\
          a warranty) and that users may redistribute the program under<br>\
          these conditions, and telling the user how to view a copy of this<br>\
          License.  (Exception: if the Program itself is interactive but<br>\
          does not normally print such an announcement, your work based on<br>\
          the Program is not required to print an announcement.)<br>\
          <br>\
          These requirements apply to the modified work as a whole.  If<br>\
          identifiable sections of that work are not derived from the Program,<br>\
          and can be reasonably considered independent and separate works in<br>\
          themselves, then this License, and its terms, do not apply to those<br>\
          sections when you distribute them as separate works.  But when you<br>\
          distribute the same sections as part of a whole which is a work based<br>\
          on the Program, the distribution of the whole must be on the terms of<br>\
          this License, whose permissions for other licensees extend to the<br>\
          entire whole, and thus to each and every part regardless of who wrote it.<br>\
          <br>\
          Thus, it is not the intent of this section to claim rights or contest<br>\
          your rights to work written entirely by you; rather, the intent is to<br>\
          exercise the right to control the distribution of derivative or<br>\
          collective works based on the Program.<br>\
          <br>\
          In addition, mere aggregation of another work not based on the Program<br>\
          with the Program (or with a work based on the Program) on a volume of<br>\
          a storage or distribution medium does not bring the other work under<br>\
          the scope of this License.<br>\
          <br>\
          3. You may copy and distribute the Program (or a work based on it,<br>\
          under Section 2) in object code or executable form under the terms of<br>\
          Sections 1 and 2 above provided that you also do one of the following:<br>\
          <br>\
          a) Accompany it with the complete corresponding machine-readable<br>\
          source code, which must be distributed under the terms of Sections<br>\
          1 and 2 above on a medium customarily used for software interchange; or,<br>\
          <br>\
          b) Accompany it with a written offer, valid for at least three<br>\
          years, to give any third party, for a charge no more than your<br>\
          cost of physically performing source distribution, a complete<br>\
          machine-readable copy of the corresponding source code, to be<br>\
          distributed under the terms of Sections 1 and 2 above on a medium<br>\
          customarily used for software interchange; or,<br>\
          <br>\
          c) Accompany it with the information you received as to the offer<br>\
          to distribute corresponding source code.  (This alternative is<br>\
          allowed only for noncommercial distribution and only if you<br>\
          received the program in object code or executable form with such<br>\
          an offer, in accord with Subsection b above.)<br>\
          <br>\
          The source code for a work means the preferred form of the work for<br>\
          making modifications to it.  For an executable work, complete source<br>\
          code means all the source code for all modules it contains, plus any<br>\
          associated interface definition files, plus the scripts used to<br>\
          control compilation and installation of the executable.  However, as a<br>\
          special exception, the source code distributed need not include<br>\
          anything that is normally distributed (in either source or binary<br>\
          form) with the major components (compiler, kernel, and so on) of the<br>\
          operating system on which the executable runs, unless that component<br>\
          itself accompanies the executable.<br>\
          <br>\
          If distribution of executable or object code is made by offering<br>\
          access to copy from a designated place, then offering equivalent<br>\
          access to copy the source code from the same place counts as<br>\
          distribution of the source code, even though third parties are not<br>\
          compelled to copy the source along with the object code.<br>\
          <br>\
          4. You may not copy, modify, sublicense, or distribute the Program<br>\
          except as expressly provided under this License.  Any attempt<br>\
          otherwise to copy, modify, sublicense or distribute the Program is<br>\
          void, and will automatically terminate your rights under this License.<br>\
          However, parties who have received copies, or rights, from you under<br>\
          this License will not have their licenses terminated so long as such<br>\
          parties remain in full compliance.<br>\
          <br>\
          5. You are not required to accept this License, since you have not<br>\
          signed it.  However, nothing else grants you permission to modify or<br>\
          distribute the Program or its derivative works.  These actions are<br>\
          prohibited by law if you do not accept this License.  Therefore, by<br>\
          modifying or distributing the Program (or any work based on the<br>\
          Program), you indicate your acceptance of this License to do so, and<br>\
          all its terms and conditions for copying, distributing or modifying<br>\
          the Program or works based on it.<br>\
          <br>\
          6. Each time you redistribute the Program (or any work based on the<br>\
          Program), the recipient automatically receives a license from the<br>\
          original licensor to copy, distribute or modify the Program subject to<br>\
          these terms and conditions.  You may not impose any further<br>\
          restrictions on the recipients' exercise of the rights granted herein.<br>\
          You are not responsible for enforcing compliance by third parties to<br>\
          this License.<br>\
          <br>\
          7. If, as a consequence of a court judgment or allegation of patent<br>\
          infringement or for any other reason (not limited to patent issues),<br>\
          conditions are imposed on you (whether by court order, agreement or<br>\
          otherwise) that contradict the conditions of this License, they do not<br>\
          excuse you from the conditions of this License.  If you cannot<br>\
          distribute so as to satisfy simultaneously your obligations under this<br>\
          License and any other pertinent obligations, then as a consequence you<br>\
          may not distribute the Program at all.  For example, if a patent<br>\
          license would not permit royalty-free redistribution of the Program by<br>\
          all those who receive copies directly or indirectly through you, then<br>\
          the only way you could satisfy both it and this License would be to<br>\
          refrain entirely from distribution of the Program.<br>\
          <br>\
          If any portion of this section is held invalid or unenforceable under<br>\
          any particular circumstance, the balance of the section is intended to<br>\
          apply and the section as a whole is intended to apply in other<br>\
          circumstances.<br>\
          <br>\
          It is not the purpose of this section to induce you to infringe any<br>\
          patents or other property right claims or to contest validity of any<br>\
          such claims; this section has the sole purpose of protecting the<br>\
          integrity of the free software distribution system, which is<br>\
          implemented by public license practices.  Many people have made<br>\
          generous contributions to the wide range of software distributed<br>\
          through that system in reliance on consistent application of that<br>\
          system; it is up to the author/donor to decide if he or she is willing<br>\
          to distribute software through any other system and a licensee cannot<br>\
          impose that choice.<br>\
          <br>\
          This section is intended to make thoroughly clear what is believed to<br>\
          be a consequence of the rest of this License.<br>\
          <br>\
          8. If the distribution and/or use of the Program is restricted in<br>\
          certain countries either by patents or by copyrighted interfaces, the<br>\
          original copyright holder who places the Program under this License<br>\
          may add an explicit geographical distribution limitation excluding<br>\
          those countries, so that distribution is permitted only in or among<br>\
          countries not thus excluded.  In such case, this License incorporates<br>\
          the limitation as if written in the body of this License.<br>\
          <br>\
          9. The Free Software Foundation may publish revised and/or new versions<br>\
          of the General Public License from time to time.  Such new versions will<br>\
          be similar in spirit to the present version, but may differ in detail to<br>\
          address new problems or concerns.<br>\
          <br>\
          Each version is given a distinguishing version number.  If the Program<br>\
          specifies a version number of this License which applies to it and 'any<br>\
          later version', you have the option of following the terms and conditions<br>\
          either of that version or of any later version published by the Free<br>\
          Software Foundation.  If the Program does not specify a version number of<br>\
          this License, you may choose any version ever published by the Free Software<br>\
          Foundation.<br>\
          <br>\
          10. If you wish to incorporate parts of the Program into other free<br>\
          programs whose distribution conditions are different, write to the author<br>\
          to ask for permission.  For software which is copyrighted by the Free<br>\
          Software Foundation, write to the Free Software Foundation; we sometimes<br>\
          make exceptions for this.  Our decision will be guided by the two goals<br>\
          of preserving the free status of all derivatives of our free software and<br>\
          of promoting the sharing and reuse of software generally.<br>\
          <br>\
          <center><b>NO WARRANTY</b></center><br>\
          11. BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY<br>\
          FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN<br>\
          OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES<br>\
          PROVIDE THE PROGRAM \"AS IS\" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED<br>\
          OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF<br>\
          MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS<br>\
          TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE<br>\
          PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,<br>\
          REPAIR OR CORRECTION.<br>\
          <br>\
          12. IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING<br>\
          WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR<br>\
          REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,<br>\
          INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING<br>\
          OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED<br>\
          TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY<br>\
          YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER<br>\
          PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE<br>\
          POSSIBILITY OF SUCH DAMAGES.<br>\
          <br>\
          <center><b>END OF TERMS AND CONDITIONS</b></center><br>\
          <center>How to Apply These Terms to Your New Programs</center><br>\
          If you develop a new program, and you want it to be of the greatest<br>\
          possible use to the public, the best way to achieve this is to make it<br>\
          free software which everyone can redistribute and change under these terms.<br>\
          <br>\
          To do so, attach the following notices to the program.  It is safest<br>\
          to attach them to the start of each source file to most effectively<br>\
          convey the exclusion of warranty; and each file should have at least<br>\
          the 'copyright' line and a pointer to where the full notice is found.<br>\
          <br>\
          <one line to give the program's name and a brief idea of what it does.><br>\
          Copyright (C) <year>  <name of author><br>\
          <br>\
          This program is free software; you can redistribute it and/or modify<br>\
          it under the terms of the GNU General Public License as published by<br>\
          the Free Software Foundation; either version 2 of the License, or<br>\
          (at your option) any later version.<br>\
          <br>\
          This program is distributed in the hope that it will be useful,<br>\
          but WITHOUT ANY WARRANTY; without even the implied warranty of<br>\
          MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the<br>\
          GNU General Public License for more details.<br>\
          <br>\
          You should have received a copy of the GNU General Public License<br>\
          along with this program; if not, write to the Free Software<br>\
          Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA<br>\
          <br>\
          <br>\
          Also add information on how to contact you by electronic and paper mail.<br>\
          <br>\
          If the program is interactive, make it output a short notice like this<br>\
          when it starts in an interactive mode:<br>\
          <br>\
          Gnomovision version 69, Copyright (C) year name of author<br>\
          Gnomovision comes with ABSOLUTELY NO WARRANTY; for details type `show w'.<br>\
          This is free software, and you are welcome to redistribute it<br>\
          under certain conditions; type `show c' for details.<br>\
          <br>\
          The hypothetical commands `show w' and `show c' should show the appropriate<br>\
          parts of the General Public License.  Of course, the commands you use may<br>\
          be called something other than `show w' and `show c'; they could even be<br>\
          mouse-clicks or menu items--whatever suits your program.<br>\
          <br>\
          You should also get your employer (if you work as a programmer) or your<br>\
          school, if any, to sign a 'copyright disclaimer' for the program, if<br>\
          necessary.  Here is a sample; alter the names:<br>\
          <br>\
          Yoyodyne, Inc., hereby disclaims all copyright interest in the program<br>\
          `Gnomovision' (which makes passes at compilers) written by James Hacker.<br>\
          <br>\
          'signature of Ty Coon', 1 April 1989<br>\
          Ty Coon, President of Vice<br>\
          <br>\
          This General Public License does not permit incorporating your program into<br>\
          proprietary programs.  If your program is a subroutine library, you may<br>\
          consider it more useful to permit linking proprietary applications with the<br>\
          library.  If this is what you want to do, use the GNU Library General<br>\
          Public License instead of this License.<br>");
          show();
    }
};

#endif
