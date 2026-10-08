#pragma once
#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <functional>
// The graphics cards of this computer and where to get their driver.
namespace GpuInfo {
struct Gpu {
    QString name, vendor;      // "NVIDIA GeForce RTX 5060", "NVIDIA" / "AMD" / "Intel" / "" (unknown or virtual)
    bool integrated = false;   // part of the processor (Intel UHD/Iris, AMD Radeon Graphics/780M…)
    bool virtualAdapter = false; // virtual machine, remote desktop or the Windows basic display driver
};
struct Computer { QString manufacturer, model; };
QList<Gpu> detect();
Computer computer();
Gpu describe(const QString &name);
// The card a crash in vendor's driver points at; otherwise the dedicated card, otherwise the first one.
Gpu pick(const QList<Gpu> &gpus, const QString &vendor = {});
struct DriverLink {
    QString url, title;
    bool exact = false;  // the page of this card's driver; otherwise the maker's automatic detection tool
};
using Fetch = std::function<QByteArray(const QUrl &)>;
// Asks NVIDIA and AMD for this card's driver page (network: call it off the UI thread). Never empty on Windows:
// when the exact page is not found, the maker's tool that finds the driver by itself.
// system: "windows", "linux" or "macos" (this computer by default).
DriverLink driverLink(const Gpu &gpu, Fetch fetch, QString system = {});
// The maker's automatic driver tool.
DriverLink autoDetect(const QString &vendor);
// Support page of the computer maker (laptops often need its customized driver); empty url when unknown.
DriverLink manufacturerSupport(const Computer &computer);
}
