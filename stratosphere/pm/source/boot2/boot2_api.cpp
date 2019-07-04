/*
 * Copyright (c) 2018-2019 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cctype>
#include <dirent.h>
#include <stratosphere/cfg.hpp>
#include <stratosphere/ldr.hpp>
#include <stratosphere/pm.hpp>
#include "boot2_api.hpp"

namespace sts::boot2 {

    namespace {

        /* Launch lists. */

        /* psc, bus, pcv is the minimal set of required titles to get SD card. */
        /* bus depends on pcie, and pcv depends on settings. */
        constexpr ncm::TitleId PreSdCardLaunchPrograms[] = {
            ncm::TitleId::Psc,         /* psc */
            ncm::TitleId::Pcie,        /* pcie */
            ncm::TitleId::Bus,         /* bus */
            ncm::TitleId::Settings,    /* settings */
            ncm::TitleId::Pcv,         /* pcv */
        };
        constexpr size_t NumPreSdCardLaunchPrograms = util::size(PreSdCardLaunchPrograms);

        constexpr ncm::TitleId AdditionalLaunchPrograms[] = {
            ncm::TitleId::Usb,         /* usb */
            ncm::TitleId::Tma,         /* tma */
            ncm::TitleId::Am,          /* am */
            ncm::TitleId::NvServices,  /* nvservices */
            ncm::TitleId::NvnFlinger,  /* nvnflinger */
            ncm::TitleId::Vi,          /* vi */
            ncm::TitleId::Ns,          /* ns */
            ncm::TitleId::LogManager,  /* lm */
            ncm::TitleId::Ppc,         /* ppc */
            ncm::TitleId::Ptm,         /* ptm */
            ncm::TitleId::Hid,         /* hid */
            ncm::TitleId::Audio,       /* audio */
            ncm::TitleId::Lbl,         /* lbl */
            ncm::TitleId::Wlan,        /* wlan */
            ncm::TitleId::Bluetooth,   /* bluetooth */
            ncm::TitleId::BsdSockets,  /* bsdsockets */
            ncm::TitleId::Nifm,        /* nifm */
            ncm::TitleId::Ldn,         /* ldn */
            ncm::TitleId::Account,     /* account */
            ncm::TitleId::Friends,     /* friends */
            ncm::TitleId::Nfc,         /* nfc */
            ncm::TitleId::JpegDec,     /* jpegdec */
            ncm::TitleId::CapSrv,      /* capsrv */
            ncm::TitleId::Ssl,         /* ssl */
            ncm::TitleId::Nim,         /* nim */
            ncm::TitleId::Bcat,        /* bcat */
            ncm::TitleId::Erpt,        /* erpt */
            ncm::TitleId::Es,          /* es */
            ncm::TitleId::Pctl,        /* pctl */
            ncm::TitleId::Btm,         /* btm */
            ncm::TitleId::Eupld,       /* eupld */
            ncm::TitleId::Glue,        /* glue */
         /* ncm::TitleId::Eclct, */    /* eclct */      /* Skip launching error collection in Atmosphere to lessen telemetry. */
            ncm::TitleId::Npns,        /* npns */
            ncm::TitleId::Fatal,       /* fatal */
            ncm::TitleId::Ro,          /* ro */
            ncm::TitleId::Profiler,    /* profiler */
            ncm::TitleId::Sdb,         /* sdb */
            ncm::TitleId::Migration,   /* migration */
            ncm::TitleId::Grc,         /* grc */
            ncm::TitleId::Olsc,        /* olsc */
        };
        constexpr size_t NumAdditionalLaunchPrograms = util::size(AdditionalLaunchPrograms);

        constexpr ncm::TitleId AdditionalMaintenanceLaunchPrograms[] = {
            ncm::TitleId::Usb,         /* usb */
            ncm::TitleId::Tma,         /* tma */
            ncm::TitleId::Am,          /* am */
            ncm::TitleId::NvServices,  /* nvservices */
            ncm::TitleId::NvnFlinger,  /* nvnflinger */
            ncm::TitleId::Vi,          /* vi */
            ncm::TitleId::Ns,          /* ns */
            ncm::TitleId::LogManager,  /* lm */
            ncm::TitleId::Ppc,         /* ppc */
            ncm::TitleId::Ptm,         /* ptm */
            ncm::TitleId::Hid,         /* hid */
            ncm::TitleId::Audio,       /* audio */
            ncm::TitleId::Lbl,         /* lbl */
            ncm::TitleId::Wlan,        /* wlan */
            ncm::TitleId::Bluetooth,   /* bluetooth */
            ncm::TitleId::BsdSockets,  /* bsdsockets */
            ncm::TitleId::Nifm,        /* nifm */
            ncm::TitleId::Ldn,         /* ldn */
            ncm::TitleId::Account,     /* account */
            ncm::TitleId::Nfc,         /* nfc */
            ncm::TitleId::JpegDec,     /* jpegdec */
            ncm::TitleId::CapSrv,      /* capsrv */
            ncm::TitleId::Ssl,         /* ssl */
            ncm::TitleId::Nim,         /* nim */
            ncm::TitleId::Erpt,        /* erpt */
            ncm::TitleId::Es,          /* es */
            ncm::TitleId::Pctl,        /* pctl */
            ncm::TitleId::Btm,         /* btm */
            ncm::TitleId::Glue,        /* glue */
         /* ncm::TitleId::Eclct, */    /* eclct */      /* Skip launching error collection in Atmosphere to lessen telemetry. */
            ncm::TitleId::Fatal,       /* fatal */
            ncm::TitleId::Ro,          /* ro */
            ncm::TitleId::Profiler,    /* profiler */
            ncm::TitleId::Sdb,         /* sdb */
            ncm::TitleId::Migration,   /* migration */
            ncm::TitleId::Grc,         /* grc */
            ncm::TitleId::Olsc,        /* olsc */
        };
        constexpr size_t NumAdditionalMaintenanceLaunchPrograms = util::size(AdditionalMaintenanceLaunchPrograms);

        /* Helpers. */
        inline bool IsHexadecimal(const char *str) {
            while (*str) {
                if (!std::isxdigit(static_cast<unsigned char>(*(str++)))) {
                    return false;
                }
            }
            return true;
        }

        void LaunchTitle(u64 *out_process_id, const ncm::TitleLocation &loc, u32 launch_flags) {
            u64 process_id = 0;

            switch (pm::shell::LaunchTitle(&process_id, loc, launch_flags)) {
                case ResultKernelResourceExhausted:
                    /* Out of resource! */
                    std::abort();
                case ResultKernelOutOfMemory:
                    /* Out of memory! */
                    std::abort();
                case ResultKernelLimitReached:
                    /* Limit Reached! */
                    std::abort();
                default:
                    /* We don't care about other issues. */
                    break;
            }

            if (out_process_id) {
                *out_process_id = process_id;
            }
        }

        void LaunchList(const ncm::TitleId *launch_list, size_t num_entries) {
            for (size_t i = 0; i < num_entries; i++) {
                LaunchTitle(nullptr, ncm::TitleLocation::Make(launch_list[i], ncm::StorageId::NandSystem), 0);
            }
        }

        bool GetGpioPadLow(GpioPadName pad) {
            GpioPadSession button;
            if (R_FAILED(gpioOpenSession(&button, pad))) {
                return false;
            }

            /* Ensure we close even on early return. */
            ON_SCOPE_EXIT { gpioPadClose(&button); };

            /* Set direction input. */
            gpioPadSetDirection(&button, GpioDirection_Input);

            GpioValue val;
            return R_SUCCEEDED(gpioPadGetValue(&button, &val)) && val == GpioValue_Low;
        }

        bool IsMaintenanceMode() {
            /* Contact set:sys, retrieve boot!force_maintenance. */
            {
                auto set_sys_holder = sm::ScopedServiceHolder<setsysInitialize, setsysExit>();
                R_ASSERT(set_sys_holder.GetResult());

                u8 force_maintenance = 1;
                setsysGetSettingsItemValue("boot", "force_maintenance", &force_maintenance, sizeof(force_maintenance));
                if (force_maintenance != 0) {
                    return true;
                }
            }

            /* Contact GPIO, read plus/minus buttons. */
            {
                auto gpio_holder = sm::ScopedServiceHolder<gpioInitialize, gpioExit>();
                R_ASSERT(gpio_holder.GetResult());

                return GetGpioPadLow(GpioPadName_ButtonVolUp) && GetGpioPadLow(GpioPadName_ButtonVolDown);
            }
        }

        void LaunchFlaggedProgramsFromSdCard() {
            /* Validate that the titles directory exists. */
            DIR *titles_dir = opendir("sdmc:/atmosphere/titles");
            if (titles_dir == nullptr) {
                return;
            }
            ON_SCOPE_EXIT { closedir(titles_dir); };

            /* Iterate over entries in the titles directory */
            struct dirent *ent;
            while ((ent = readdir(titles_dir)) != nullptr) {
                /* Check that the subdirectory can be converted to a title id. */
                if (std::strlen(ent->d_name) == 2 * sizeof(ncm::TitleId) && IsHexadecimal(ent->d_name)) {
                    /* Check if we've already launched the title. */
                    ncm::TitleId title_id{std::strtoul(ent->d_name, nullptr, 16)};
                    if (pm::info::HasLaunchedTitle(title_id)) {
                        continue;
                    }

                    /* Check if the title is flagged. */
                    char title_path[FS_MAX_PATH];
                    std::snprintf(title_path, sizeof(title_path), "sdmc:/atmosphere/titles/%s/flags/boot2.flag", ent->d_name);
                    FILE *f_flag = fopen(title_path, "rb");
                    if (f_flag == nullptr) {
                        continue;
                    }
                    fclose(f_flag);

                    /* Actually launch the title. */
                    LaunchTitle(nullptr, ncm::TitleLocation::Make(title_id, ncm::StorageId::None), 0);
                }
            }
        }

    }

    /* Boot2 API. */
    void LaunchBootPrograms() {
        /* Wait until fs.mitm has installed itself. We want this to happen as early as possible. */
        R_ASSERT(sm::mitm::WaitMitm(sm::ServiceName::Encode("fsp-srv")));

        /* Launch programs required to mount the SD card. */
        LaunchList(PreSdCardLaunchPrograms, NumPreSdCardLaunchPrograms);

        /* At this point, the SD card can be mounted. */
        cfg::WaitSdCardInitialized();
        R_ASSERT(fsdevMountSdmc());

        /* Find out whether we are maintenance mode. */
        const bool maintenance = IsMaintenanceMode();
        if (maintenance) {
            pm::bm::SetMaintenanceBoot();
        }

        /* Wait for other atmosphere mitm modules to initialize. */
        R_ASSERT(sm::mitm::WaitMitm(sm::ServiceName::Encode("set:sys")));
        if (GetRuntimeFirmwareVersion() >= FirmwareVersion_200) {
            R_ASSERT(sm::mitm::WaitMitm(sm::ServiceName::Encode("bpc")));
        } else {
            R_ASSERT(sm::mitm::WaitMitm(sm::ServiceName::Encode("bpc:c")));
        }

        /* Launch Atmosphere dmnt, using FsStorageId_None to force SD card boot. */
        LaunchTitle(nullptr, ncm::TitleLocation::Make(ncm::TitleId::Dmnt, ncm::StorageId::None), 0);

        /* Launch additional programs. */
        if (maintenance) {
            LaunchList(AdditionalMaintenanceLaunchPrograms, NumAdditionalMaintenanceLaunchPrograms);
        } else {
            LaunchList(AdditionalLaunchPrograms, NumAdditionalLaunchPrograms);
        }

        /* Launch user programs off of the SD. */
        LaunchFlaggedProgramsFromSdCard();

        /* We no longer need the SD card. */
        fsdevUnmountAll();
    }

}
