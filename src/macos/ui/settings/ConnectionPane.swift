/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  svbmwjwj <255624542+svbmwjwj@users.noreply.github.com>
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
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

import QBTBridge
import SwiftUI

/// Ports, binding, connection limits, the proxy, I2P and IP filtering: the
/// Qt UI's Connection options.
struct ConnectionPane: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        let interface = setting($revision, get: { QBTSettings.networkInterface }, set: { QBTSettings.networkInterface = $0 })
        Form {
            Section {
                Picker("Peer connection protocol", selection: setting($revision, get: { QBTConnectionSettings.btProtocol }, set: { QBTConnectionSettings.btProtocol = $0 })) {
                    Text("TCP and µTP").tag(Int32(0))
                    Text("TCP").tag(Int32(1))
                    Text("µTP").tag(Int32(2))
                }
                LabeledContent("Listening port") {
                    TextField("Listening port", value: setting($revision, get: { Int(QBTSettings.listeningPort) }, set: { QBTSettings.listeningPort = Int32(clamping: $0) }), format: .number.grouping(.never))
                        .labelsHidden()
                        .multilineTextAlignment(.trailing)
                        .frame(width: 90)
                }
                Toggle("Forward the port with UPnP / NAT-PMP", isOn: setting($revision, get: { QBTSettings.portForwardingEnabled }, set: { QBTSettings.portForwardingEnabled = $0 }))
            }
            Section {
                Picker("Network interface", selection: interface) {
                    Text("Any interface").tag("")
                    ForEach(interfaces(current: interface.wrappedValue), id: \.self) { Text($0).tag($0) }
                }
                Picker("Address", selection: setting($revision, get: { QBTSettings.networkInterfaceAddress }, set: { QBTSettings.networkInterfaceAddress = $0 })) {
                    Text("All addresses").tag("")
                    Text("All IPv4 addresses").tag("0.0.0.0")
                    Text("All IPv6 addresses").tag("::")
                    ForEach(addresses(interface: interface.wrappedValue), id: \.self) { Text($0).tag($0) }
                }
            } header: {
                Text("Binding")
            } footer: {
                Text("Binding to the physical interface, such as en0, keeps torrent traffic off VPNs and TUN-mode proxies.")
            }
            Section("Connection limits") {
                LimitField(title: "Global connections", value: limit(setting($revision, get: { QBTSettings.maxConnections }, set: { QBTSettings.maxConnections = $0 })))
                LimitField(title: "Connections per torrent", value: limit(setting($revision, get: { QBTSettings.maxConnectionsPerTorrent }, set: { QBTSettings.maxConnectionsPerTorrent = $0 })))
                LimitField(title: "Global upload slots", value: limit(setting($revision, get: { QBTSettings.maxUploads }, set: { QBTSettings.maxUploads = $0 })))
                LimitField(title: "Upload slots per torrent", value: limit(setting($revision, get: { QBTSettings.maxUploadsPerTorrent }, set: { QBTSettings.maxUploadsPerTorrent = $0 })))
            }
            ProxySection()
            if QBTConnectionSettings.i2pAvailable {
                i2pSection
            }
            IPFilterSection()
        }
    }

    private var i2pSection: some View {
        Section {
            Toggle("Connect to I2P peers", isOn: setting($revision, get: { QBTConnectionSettings.i2pEnabled }, set: { QBTConnectionSettings.i2pEnabled = $0 }))
            if QBTConnectionSettings.i2pEnabled {
                TextField("Host", text: setting($revision, get: { QBTConnectionSettings.i2pHost }, set: { QBTConnectionSettings.i2pHost = $0 }))
                TextField("Port", value: setting($revision, get: { Int(QBTConnectionSettings.i2pPort) }, set: { QBTConnectionSettings.i2pPort = Int32(clamping: $0) }), format: .number.grouping(.never))
                Toggle("Mixed mode", isOn: setting($revision, get: { QBTConnectionSettings.i2pMixedMode }, set: { QBTConnectionSettings.i2pMixedMode = $0 }))
            }
        } header: {
            Text("I2P (experimental)")
        } footer: {
            Text("Needs an I2P router with its SAM bridge on. In mixed mode, I2P torrents also connect to peers outside I2P, which are not anonymous.")
        }
    }

    private func interfaces(current: String) -> [String] {
        var names = QBTSettings.availableNetworkInterfaces
        // A saved interface that is not up now still shows.
        if !current.isEmpty, !names.contains(current) { names.append(current) }
        return names
    }

    private func addresses(interface: String) -> [String] {
        var addresses = QBTSettings.addresses(ofNetworkInterface: interface)
        let current = QBTSettings.networkInterfaceAddress
        if !["", "0.0.0.0", "::"].contains(current), !addresses.contains(current) { addresses.append(current) }
        return addresses
    }

    /// The core stores "unlimited" as -1; the field shows it as empty or 0.
    private func limit(_ binding: Binding<Int32>) -> Binding<Int> {
        Binding { max(0, Int(binding.wrappedValue)) } set: { binding.wrappedValue = Int32(clamping: $0) }
    }
}

private struct ProxySection: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        let type = setting($revision, get: { QBTSettings.proxyType }, set: { QBTSettings.proxyType = $0 })
        let isSOCKS4 = type.wrappedValue == 5
        Section {
            Picker("Type", selection: type) {
                Text("None").tag(Int32(0))
                Text("HTTP").tag(Int32(1))
                Text("SOCKS5").tag(Int32(2))
                Text("SOCKS4").tag(Int32(5))
            }
            if type.wrappedValue != 0 {
                TextField("Host", text: setting($revision, get: { QBTSettings.proxyHost }, set: { QBTSettings.proxyHost = $0 }))
                TextField("Port", value: setting($revision, get: { Int(QBTSettings.proxyPort) }, set: { QBTSettings.proxyPort = Int32(clamping: $0) }), format: .number.grouping(.never))
                Toggle("Look up host names through the proxy", isOn: setting($revision, get: { QBTSettings.proxyHostnameLookupEnabled }, set: { QBTSettings.proxyHostnameLookupEnabled = $0 }))
                    .disabled(isSOCKS4)
                Toggle("Authentication", isOn: setting($revision, get: { QBTSettings.proxyAuthEnabled }, set: { QBTSettings.proxyAuthEnabled = $0 }))
                    .disabled(isSOCKS4)
                if QBTSettings.proxyAuthEnabled, !isSOCKS4 {
                    TextField("Username", text: setting($revision, get: { QBTSettings.proxyUsername }, set: { QBTSettings.proxyUsername = $0 }))
                    SecureField("Password", text: setting($revision, get: { QBTSettings.proxyPassword }, set: { QBTSettings.proxyPassword = $0 }))
                }
                Toggle("Use the proxy for BitTorrent", isOn: setting($revision, get: { QBTConnectionSettings.proxyForBitTorrent }, set: { QBTConnectionSettings.proxyForBitTorrent = $0 }))
                Toggle("Use the proxy for peer connections", isOn: setting($revision, get: { QBTSettings.proxyPeerConnectionsEnabled }, set: { QBTSettings.proxyPeerConnectionsEnabled = $0 }))
                    .disabled(!QBTConnectionSettings.proxyForBitTorrent)
                Toggle("Use the proxy for RSS feeds", isOn: setting($revision, get: { QBTConnectionSettings.proxyForRSS }, set: { QBTConnectionSettings.proxyForRSS = $0 }))
                    .disabled(isSOCKS4)
                Toggle("Use the proxy for search and other web requests", isOn: setting($revision, get: { QBTConnectionSettings.proxyForGeneralPurposes }, set: { QBTConnectionSettings.proxyForGeneralPurposes = $0 }))
                    .disabled(isSOCKS4)
            }
        } header: {
            Text("Proxy")
        } footer: {
            if type.wrappedValue != 0 {
                if isSOCKS4 {
                    Text("SOCKS4 does not support host name lookup, authentication, RSS feeds or other web requests.")
                } else {
                    Text("Without peer connections, BitTorrent uses the proxy only for trackers.")
                }
            }
        }
    }
}
