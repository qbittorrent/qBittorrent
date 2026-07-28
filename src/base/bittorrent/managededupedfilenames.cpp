#include "managededupedfilenames.h"

#include <libtorrent/torrent_info.hpp>
#include <libtorrent/write_resume_data.hpp>

#include <QCoreApplication>
#include <QDebug>

#include "base/logger.h"
#include "infohash.h"

namespace BitTorrent
{
    // Makes sure renamed_files properly accounts for libtorrent filename deduplication based on DedupeAction value.
    //
    // Revert:
    // Compares orig_files() to files() and packs divergent orig_files() into renamed_files if not covered.
    // (Fixes "Missing Files" errors caused by libtorrent filename mutations.)
    //
    // Insert:
    // Compares orig_files() to files() and packs divergent files() into renamed_files if not covered.
    // (Allows use of libtorrent deduplicated filenames when adding a new torrent from a .torrent file.)
    //
    // MagnetInsert:
    // Creates a temp .ti to force duplication check, then packs divergent files() into renamed_files if not covered.
    // (Allows use of libtorrent deduplicated filenames when adding a new torrent from a magnet link.)
    int manageDedupedFilenames(lt::add_torrent_params &atp, const DedupeAction action)
    {
        int mutation_counter = 0;
        if (!atp.ti || !atp.ti->is_valid())
            return mutation_counter;

        std::shared_ptr<const lt::torrent_info> deduped_ti;
        const lt::file_storage *original = nullptr;
        const lt::file_storage *deduped = nullptr;
        if (action == DedupeAction::MagnetInsert)
        {
            const lt::span<char const> buffer = lt::write_torrent_file_buf(atp, lt::write_flags::allow_missing_piece_layer);
            if (buffer.empty())
                return mutation_counter;

            lt::error_code ec;
            deduped_ti = std::make_shared<lt::torrent_info>(buffer.data(), static_cast<int>(buffer.size()), ec);
            if (ec)
                return mutation_counter;

#if LIBTORRENT_VERSION_NUM >= 20100
            original = &deduped_ti->layout();
            deduped = &deduped_ti->files_impl();
#else
            original = &deduped_ti->orig_files();
            deduped = &deduped_ti->files();
#endif
        }
        else
        {
#if LIBTORRENT_VERSION_NUM >= 20100
            original = &atp.ti->layout();
            deduped = &atp.ti->files_impl();
#else
            original = &atp.ti->orig_files();
            deduped = &atp.ti->files();
#endif
        }

        if (original == deduped)
            return mutation_counter;

        for (const lt::file_index_t file_index : original->file_range())
        {
            if (original->pad_file_at(file_index))
                continue;
            if (original->file_name(file_index) == deduped->file_name(file_index))
                continue;

            const auto it = atp.renamed_files.lower_bound(file_index);
            if (it == atp.renamed_files.end() || file_index < it->first)
            {
                ++mutation_counter;
                std::string correction = (action == DedupeAction::Revert) ? original->file_path(file_index) : deduped->file_path(file_index);
                const auto result = atp.renamed_files.emplace_hint(it, file_index, std::move(correction));
                qDebug() << "Adding" << atp.ti->name() << "index" << int(file_index) << "to renamed_files:" << result->second;
            }
        }

        if (mutation_counter)
        {
#ifdef QBT_USES_LIBTORRENT2
            const QString info_hash = BitTorrent::InfoHash(atp.ti->info_hashes()).toString();
#else
            const QString info_hash = BitTorrent::InfoHash(atp.ti->info_hash()).toString();
#endif
            LogMsg(QCoreApplication::translate("manageDedupedFilenames", "%1 mutated filenames detected loading torrent: %2 {%3}").arg(mutation_counter).arg(atp.ti->name()).arg(info_hash), Log::WARNING);
        }
        return mutation_counter;
    }
}
