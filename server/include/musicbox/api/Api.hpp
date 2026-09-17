#pragma once

#include "musicbox/db/AlbumRepository.hpp"
#include "musicbox/db/ArtistRepository.hpp"
#include "musicbox/db/LibraryRootRepository.hpp"
#include "musicbox/db/PlaylistRepository.hpp"
#include "musicbox/db/TrackRepository.hpp"
#include "musicbox/http/Router.hpp"

namespace musicbox::api {

// Registers every /api/v1/* route documented in docs/api.md against `router`,
// backed by the given repositories. This is the API layer's composition
// entry point -- the caller (main(), or a test) owns the repository
// instances and the router for as long as the router may be dispatched
// against; this function only wires handlers as closures over references to
// them, it doesn't construct or take ownership of anything.
void registerApiRoutes(musicbox::http::Router& router, musicbox::db::TrackRepository& trackRepo,
                       musicbox::db::ArtistRepository& artistRepo,
                       musicbox::db::AlbumRepository& albumRepo,
                       musicbox::db::PlaylistRepository& playlistRepo,
                       musicbox::db::LibraryRootRepository& libraryRootRepo);

} // namespace musicbox::api
