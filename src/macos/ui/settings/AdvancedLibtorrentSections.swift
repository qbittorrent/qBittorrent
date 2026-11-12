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

/// The libtorrent section of the Qt UI's Advanced options. Options the
/// libtorrent in use lacks are left out, as in the Qt UI.
struct LibtorrentSections: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        diskSection
        connectionsSection
        trackersSection
        if QBTAdvancedSettings.i2pOptionsAvailable {
            i2pSection
        }
    }

    private var diskSection: some View {
        Section {
            AdvancedNumberField(title: "Bdecode depth limit", value: setting($revision, get: { QBTAdvancedSettings.bdecodeDepthLimit }, set: { QBTAdvancedSettings.bdecodeDepthLimit = $0 }), range: 0...Int32.max)
            AdvancedNumberField(title: "Bdecode token limit", value: setting($revision, get: { QBTAdvancedSettings.bdecodeTokenLimit }, set: { QBTAdvancedSettings.bdecodeTokenLimit = $0 }), range: 0...Int32.max)
            if QBTAdvancedSettings.usesLibtorrent21 {
                AdvancedNumberField(title: "Maximum directory depth when loading torrents", value: setting($revision, get: { QBTAdvancedSettings.loadTorrentMaxDirectoryDepth }, set: { QBTAdvancedSettings.loadTorrentMaxDirectoryDepth = $0 }), range: 0...Int32.max)
            }
            AdvancedNumberField(title: "Asynchronous I/O threads", value: setting($revision, get: { QBTAdvancedSettings.asyncIOThreads }, set: { QBTAdvancedSettings.asyncIOThreads = $0 }), range: 1...1024)
            if QBTAdvancedSettings.usesLibtorrent2 {
                AdvancedNumberField(title: "Hashing threads", value: setting($revision, get: { QBTAdvancedSettings.hashingThreads }, set: { QBTAdvancedSettings.hashingThreads = $0 }), range: 1...1024)
            }
            AdvancedNumberField(title: "File pool size", value: setting($revision, get: { QBTAdvancedSettings.filePoolSize }, set: { QBTAdvancedSettings.filePoolSize = $0 }), range: 1...Int32.max)
            AdvancedNumberField(title: "Outstanding memory when checking torrents", value: setting($revision, get: { QBTAdvancedSettings.checkingMemUsage }, set: { QBTAdvancedSettings.checkingMemUsage = $0 }), range: 1...QBTAdvancedSettings.checkingMemUsageMaximum, unit: "MiB")
            if !QBTAdvancedSettings.usesLibtorrent2 {
                AdvancedNumberField(title: "Disk cache", value: setting($revision, get: { QBTAdvancedSettings.diskCacheSize }, set: { QBTAdvancedSettings.diskCacheSize = $0 }), range: -1...QBTAdvancedSettings.diskCacheSizeMaximum, unit: "MiB", note: "-1: automatic, 0: disabled")
                AdvancedNumberField(title: "Disk cache expiry interval", value: setting($revision, get: { QBTAdvancedSettings.diskCacheTTL }, set: { QBTAdvancedSettings.diskCacheTTL = $0 }), range: 1...Int32.max, unit: "s")
            }
            AdvancedNumberField(title: "Disk queue size", value: setting($revision, get: { QBTAdvancedSettings.diskQueueSize }, set: { QBTAdvancedSettings.diskQueueSize = $0 }), range: 1...(Int32.max / 1024), unit: "KiB")
            if QBTAdvancedSettings.usesLibtorrent2 {
                Picker(selection: setting($revision, get: { QBTAdvancedSettings.diskIOType }, set: { QBTAdvancedSettings.diskIOType = $0 })) {
                    Text("Default").tag(Int32(0))
                    Text("Memory mapped files").tag(Int32(1))
                    Text("POSIX-compliant").tag(Int32(2))
                    Text("Simple pread/pwrite").tag(Int32(3))
                    if QBTAdvancedSettings.usesLibtorrent21 {
                        Text("Pread/pwrite").tag(Int32(4))
                    }
                } label: {
                    Text("Disk IO type")
                    Text("Takes effect after a restart.")
                }
            }
            Picker("Disk IO read mode", selection: setting($revision, get: { QBTAdvancedSettings.diskIOReadMode }, set: { QBTAdvancedSettings.diskIOReadMode = $0 })) {
                Text("Disable OS cache").tag(Int32(0))
                Text("Enable OS cache").tag(Int32(1))
            }
            Picker("Disk IO write mode", selection: setting($revision, get: { QBTAdvancedSettings.diskIOWriteMode }, set: { QBTAdvancedSettings.diskIOWriteMode = $0 })) {
                Text("Disable OS cache").tag(Int32(0))
                Text("Enable OS cache").tag(Int32(1))
                if QBTAdvancedSettings.usesLibtorrent2 {
                    Text("Write-through").tag(Int32(2))
                }
            }
            if !QBTAdvancedSettings.usesLibtorrent2 {
                Toggle("Coalesce reads & writes", isOn: setting($revision, get: { QBTAdvancedSettings.coalesceReadWriteEnabled }, set: { QBTAdvancedSettings.coalesceReadWriteEnabled = $0 }))
            }
            Toggle("Use piece extent affinity", isOn: setting($revision, get: { QBTAdvancedSettings.pieceExtentAffinity }, set: { QBTAdvancedSettings.pieceExtentAffinity = $0 }))
            Toggle("Send upload piece suggestions", isOn: setting($revision, get: { QBTAdvancedSettings.suggestMode }, set: { QBTAdvancedSettings.suggestMode = $0 }))
            AdvancedNumberField(title: "Send buffer watermark", value: setting($revision, get: { QBTAdvancedSettings.sendBufferWatermark }, set: { QBTAdvancedSettings.sendBufferWatermark = $0 }), range: 1...Int32.max, unit: "KiB")
            AdvancedNumberField(title: "Send buffer low watermark", value: setting($revision, get: { QBTAdvancedSettings.sendBufferLowWatermark }, set: { QBTAdvancedSettings.sendBufferLowWatermark = $0 }), range: 1...Int32.max, unit: "KiB")
            AdvancedNumberField(title: "Send buffer watermark factor", value: setting($revision, get: { QBTAdvancedSettings.sendBufferWatermarkFactor }, set: { QBTAdvancedSettings.sendBufferWatermarkFactor = $0 }), range: 1...Int32.max, unit: "%")
        } header: {
            Text("libtorrent")
        } footer: {
            Link("Open documentation", destination: URL(string: "https://www.libtorrent.org/reference-Settings.html")!)
        }
    }

    private var connectionsSection: some View {
        Section("libtorrent: connections") {
            AdvancedNumberField(title: "Outgoing connections per second", value: setting($revision, get: { QBTAdvancedSettings.connectionSpeed }, set: { QBTAdvancedSettings.connectionSpeed = $0 }), range: 0...Int32.max)
            Toggle("Allow outgoing connections when seeding", isOn: setting($revision, get: { QBTAdvancedSettings.seedingOutgoingConnections }, set: { QBTAdvancedSettings.seedingOutgoingConnections = $0 }))
            AdvancedNumberField(title: "Socket send buffer size", value: setting($revision, get: { QBTAdvancedSettings.socketSendBufferSize }, set: { QBTAdvancedSettings.socketSendBufferSize = $0 }), range: 0...(Int32.max / 1024), unit: "KiB", note: "0: system default")
            AdvancedNumberField(title: "Socket receive buffer size", value: setting($revision, get: { QBTAdvancedSettings.socketReceiveBufferSize }, set: { QBTAdvancedSettings.socketReceiveBufferSize = $0 }), range: 0...(Int32.max / 1024), unit: "KiB", note: "0: system default")
            AdvancedNumberField(title: "Socket backlog size", value: setting($revision, get: { QBTAdvancedSettings.socketBacklogSize }, set: { QBTAdvancedSettings.socketBacklogSize = $0 }), range: 1...Int32.max)
            AdvancedNumberField(title: "Outgoing ports (min)", value: setting($revision, get: { QBTAdvancedSettings.outgoingPortsMin }, set: { QBTAdvancedSettings.outgoingPortsMin = $0 }), range: 0...65535, note: "0: disabled")
            AdvancedNumberField(title: "Outgoing ports (max)", value: setting($revision, get: { QBTAdvancedSettings.outgoingPortsMax }, set: { QBTAdvancedSettings.outgoingPortsMax = $0 }), range: 0...65535, note: "0: disabled")
            AdvancedNumberField(title: "UPnP lease duration", value: setting($revision, get: { QBTAdvancedSettings.upnpLeaseDuration }, set: { QBTAdvancedSettings.upnpLeaseDuration = $0 }), range: 0...Int32.max, unit: "s", note: "0: permanent lease")
            AdvancedNumberField(title: "Differentiated Services Code Point (DSCP) for connections to peers", value: setting($revision, get: { QBTAdvancedSettings.peerDSCP }, set: { QBTAdvancedSettings.peerDSCP = $0 }), range: 0...255)
            Picker("µTP-TCP mixed mode algorithm", selection: setting($revision, get: { QBTAdvancedSettings.utpMixedMode }, set: { QBTAdvancedSettings.utpMixedMode = $0 })) {
                Text("Prefer TCP").tag(Int32(0))
                Text("Peer proportional (throttles TCP)").tag(Int32(1))
            }
            AdvancedNumberField(title: "Internal hostname resolver cache expiry interval", value: setting($revision, get: { QBTAdvancedSettings.hostnameCacheTTL }, set: { QBTAdvancedSettings.hostnameCacheTTL = $0 }), range: 0...Int32.max, unit: "s")
            Toggle("Support internationalized domain name (IDN)", isOn: setting($revision, get: { QBTAdvancedSettings.idnSupportEnabled }, set: { QBTAdvancedSettings.idnSupportEnabled = $0 }))
            Toggle("Allow multiple connections from the same IP address", isOn: setting($revision, get: { QBTAdvancedSettings.multiConnectionsPerIPEnabled }, set: { QBTAdvancedSettings.multiConnectionsPerIPEnabled = $0 }))
            if QBTAdvancedSettings.multiConnectionsPerPeerIDAvailable {
                Toggle("Allow multiple connections from the same peer ID", isOn: setting($revision, get: { QBTAdvancedSettings.multiConnectionsPerPeerIDEnabled }, set: { QBTAdvancedSettings.multiConnectionsPerPeerIDEnabled = $0 }))
            }
            Toggle("Validate HTTPS tracker certificates", isOn: setting($revision, get: { QBTAdvancedSettings.validateHTTPSTrackerCertificate }, set: { QBTAdvancedSettings.validateHTTPSTrackerCertificate = $0 }))
            Toggle("Server-side request forgery (SSRF) mitigation", isOn: setting($revision, get: { QBTAdvancedSettings.ssrfMitigationEnabled }, set: { QBTAdvancedSettings.ssrfMitigationEnabled = $0 }))
            Toggle("Disallow connection to peers on privileged ports", isOn: setting($revision, get: { QBTAdvancedSettings.blockPeersOnPrivilegedPorts }, set: { QBTAdvancedSettings.blockPeersOnPrivilegedPorts = $0 }))
            Picker("Upload slots behavior", selection: setting($revision, get: { QBTAdvancedSettings.chokingAlgorithm }, set: { QBTAdvancedSettings.chokingAlgorithm = $0 })) {
                Text("Fixed slots").tag(Int32(0))
                Text("Upload rate based").tag(Int32(1))
            }
            Picker("Upload choking algorithm", selection: setting($revision, get: { QBTAdvancedSettings.seedChokingAlgorithm }, set: { QBTAdvancedSettings.seedChokingAlgorithm = $0 })) {
                Text("Round-robin").tag(Int32(0))
                Text("Fastest upload").tag(Int32(1))
                Text("Anti-leech").tag(Int32(2))
            }
        }
    }

    private var trackersSection: some View {
        Section {
            Toggle("Always announce to all trackers in a tier", isOn: setting($revision, get: { QBTAdvancedSettings.announceToAllTrackers }, set: { QBTAdvancedSettings.announceToAllTrackers = $0 }))
            Toggle("Always announce to all tiers", isOn: setting($revision, get: { QBTAdvancedSettings.announceToAllTiers }, set: { QBTAdvancedSettings.announceToAllTiers = $0 }))
            AdvancedTextField(title: "IP address reported to trackers", note: "Takes effect after a restart.", get: { QBTAdvancedSettings.announceIP }, set: { QBTAdvancedSettings.announceIP = $0 })
            AdvancedNumberField(title: "Port reported to trackers", value: setting($revision, get: { QBTAdvancedSettings.announcePort }, set: { QBTAdvancedSettings.announcePort = $0 }), range: 0...65535, note: "0: listening port. Takes effect after a restart.")
            AdvancedNumberField(title: "Max concurrent HTTP announces", value: setting($revision, get: { QBTAdvancedSettings.maxConcurrentHTTPAnnounces }, set: { QBTAdvancedSettings.maxConcurrentHTTPAnnounces = $0 }), range: 0...Int32.max)
            AdvancedNumberField(title: "Stop tracker timeout", value: setting($revision, get: { QBTAdvancedSettings.stopTrackerTimeout }, set: { QBTAdvancedSettings.stopTrackerTimeout = $0 }), range: 0...Int32.max, unit: "s", note: "0: disabled")
            AdvancedNumberField(title: "Peer turnover disconnect percentage", value: setting($revision, get: { QBTAdvancedSettings.peerTurnover }, set: { QBTAdvancedSettings.peerTurnover = $0 }), range: 0...100, unit: "%")
            AdvancedNumberField(title: "Peer turnover threshold percentage", value: setting($revision, get: { QBTAdvancedSettings.peerTurnoverCutoff }, set: { QBTAdvancedSettings.peerTurnoverCutoff = $0 }), range: 0...100, unit: "%")
            AdvancedNumberField(title: "Peer turnover disconnect interval", value: setting($revision, get: { QBTAdvancedSettings.peerTurnoverInterval }, set: { QBTAdvancedSettings.peerTurnoverInterval = $0 }), range: 30...3600, unit: "s")
            AdvancedNumberField(title: "Maximum outstanding requests to a single peer", value: setting($revision, get: { QBTAdvancedSettings.requestQueueSize }, set: { QBTAdvancedSettings.requestQueueSize = $0 }), range: 1...Int32.max)
            AdvancedNumberField(title: "Maximum outstanding block requests from a peer", value: setting($revision, get: { QBTAdvancedSettings.maxOutstandingBlockRequests }, set: { QBTAdvancedSettings.maxOutstandingBlockRequests = $0 }), range: 1...Int32.max)
            AdvancedTextField(title: "DHT bootstrap nodes", prompt: "Resets to default if empty", get: { QBTAdvancedSettings.dhtBootstrapNodes }, set: { QBTAdvancedSettings.dhtBootstrapNodes = $0 })
            if QBTAdvancedSettings.usesLibtorrent21 {
                AdvancedTextField(title: "STUN server for WebTorrent NAT traversal", get: { QBTAdvancedSettings.webTorrentSTUNServer }, set: { QBTAdvancedSettings.webTorrentSTUNServer = $0 })
            }
        } header: {
            Text("libtorrent: trackers and peers")
        }
    }

    private var i2pSection: some View {
        Section("libtorrent: I2P") {
            AdvancedNumberField(title: "I2P inbound quantity", value: setting($revision, get: { QBTAdvancedSettings.i2pInboundQuantity }, set: { QBTAdvancedSettings.i2pInboundQuantity = $0 }), range: 1...16)
            AdvancedNumberField(title: "I2P outbound quantity", value: setting($revision, get: { QBTAdvancedSettings.i2pOutboundQuantity }, set: { QBTAdvancedSettings.i2pOutboundQuantity = $0 }), range: 1...16)
            AdvancedNumberField(title: "I2P inbound length", value: setting($revision, get: { QBTAdvancedSettings.i2pInboundLength }, set: { QBTAdvancedSettings.i2pInboundLength = $0 }), range: 0...7)
            AdvancedNumberField(title: "I2P outbound length", value: setting($revision, get: { QBTAdvancedSettings.i2pOutboundLength }, set: { QBTAdvancedSettings.i2pOutboundLength = $0 }), range: 0...7)
            if QBTAdvancedSettings.i2pLengthVarianceAvailable {
                AdvancedNumberField(title: "I2P inbound length variance", value: setting($revision, get: { QBTAdvancedSettings.i2pInboundLengthVariance }, set: { QBTAdvancedSettings.i2pInboundLengthVariance = $0 }), range: -7...7)
                AdvancedNumberField(title: "I2P outbound length variance", value: setting($revision, get: { QBTAdvancedSettings.i2pOutboundLengthVariance }, set: { QBTAdvancedSettings.i2pOutboundLengthVariance = $0 }), range: -7...7)
            }
        }
    }
}
