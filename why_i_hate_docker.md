# Why I Hate Docker
## The Case Against Containerizing Everything

*By Volmarr Wyrd*

> **My position in one sentence:** Docker is a specialized deployment tool that escaped its proper niche, became a cultural default, and convinced a generation of developers that adding another operating layer is somehow simpler than understanding the system they are already using.

---

## Table of Contents

1. [I Do Not Hate Containers. I Hate Docker as a Default](#i-do-not-hate-containers-i-hate-docker-as-a-default)
2. [What Docker Actually Is](#what-docker-actually-is)
3. [The Great Trick: Docker Does Not Remove Complexity](#the-great-trick-docker-does-not-remove-complexity)
4. [Docker Solves Problems Most People Do Not Have](#docker-solves-problems-most-people-do-not-have)
5. [The Architecture Is More Complicated Than It Needs to Be](#the-architecture-is-more-complicated-than-it-needs-to-be)
6. [The Daemon Model Is a Bad Default](#the-daemon-model-is-a-bad-default)
7. [Containers Are Not Virtual Machines](#containers-are-not-virtual-machines)
8. [The Isolation Is Not What Many People Think It Is](#the-isolation-is-not-what-many-people-think-it-is)
9. [Docker Does Not Magically Make Software Reproducible](#docker-does-not-magically-make-software-reproducible)
10. [Docker Does Not Magically Make Software Portable](#docker-does-not-magically-make-software-portable)
11. [Docker Does Not Eliminate Dependency Problems](#docker-does-not-eliminate-dependency-problems)
12. [Docker Networking Is Often Self-Inflicted Complexity](#docker-networking-is-often-self-inflicted-complexity)
13. [Docker Storage Turns Ordinary Files Into Infrastructure](#docker-storage-turns-ordinary-files-into-infrastructure)
14. [Permissions Become Needlessly Weird](#permissions-become-needlessly-weird)
15. [Docker Creates Its Own Disk-Space Problems](#docker-creates-its-own-disk-space-problems)
16. [The Build Cache Is Another Thing I Should Not Have to Babysit](#the-build-cache-is-another-thing-i-should-not-have-to-babysit)
17. [Resource Isolation Is Not Automatic](#resource-isolation-is-not-automatic)
18. [Docker Desktop Is Especially Absurd for Many Desktop Users](#docker-desktop-is-especially-absurd-for-many-desktop-users)
19. [Hardware Access Is Where the Illusion Starts Falling Apart](#hardware-access-is-where-the-illusion-starts-falling-apart)
20. [Stateful Services Expose the Contradictions](#stateful-services-expose-the-contradictions)
21. [Debugging Gets Worse](#debugging-gets-worse)
22. [Observability Gets More Complicated](#observability-gets-more-complicated)
23. [Docker Encourages Disposable Knowledge](#docker-encourages-disposable-knowledge)
24. [The Dockerfile Is Not the Beautiful Abstraction People Pretend It Is](#the-dockerfile-is-not-the-beautiful-abstraction-people-pretend-it-is)
25. [Compose Is Convenient Until It Becomes Its Own Operating System](#compose-is-convenient-until-it-becomes-its-own-operating-system)
26. [Image Registries Create Another Supply Chain](#image-registries-create-another-supply-chain)
27. [Docker Makes Simple Software Look Like Distributed Infrastructure](#docker-makes-simple-software-look-like-distributed-infrastructure)
28. [The Microservice Cargo Cult Made Everything Worse](#the-microservice-cargo-cult-made-everything-worse)
29. [Why I Think Developer Laziness Drives So Much of Docker Culture](#why-i-think-developer-laziness-drives-so-much-of-docker-culture)
30. [Convenience Is Not the Same Thing as Engineering](#convenience-is-not-the-same-thing-as-engineering)
31. [What I Would Do Instead for Most Software](#what-i-would-do-instead-for-most-software)
32. [Better Alternatives When I Really Do Need Isolation](#better-alternatives-when-i-really-do-need-isolation)
33. [Podman](#podman)
34. [containerd and nerdctl](#containerd-and-nerdctl)
35. [Incus and LXC](#incus-and-lxc)
36. [systemd-nspawn](#systemd-nspawn)
37. [Nix and NixOS](#nix-and-nixos)
38. [GNU Guix](#gnu-guix)
39. [Virtual Machines](#virtual-machines)
40. [Language-Native Environments](#language-native-environments)
41. [Traditional Package Managers](#traditional-package-managers)
42. [Configuration Management](#configuration-management)
43. [Flatpak and Similar Desktop Packaging](#flatpak-and-similar-desktop-packaging)
44. [When Docker Actually Makes Sense](#when-docker-actually-makes-sense)
45. [When I Would Absolutely Not Use Docker](#when-i-would-absolutely-not-use-docker)
46. [My Decision Rule](#my-decision-rule)
47. [My Preferred Deployment Philosophy](#my-preferred-deployment-philosophy)
48. [The Bottom Line](#the-bottom-line)
49. [Technical Notes and Sources](#technical-notes-and-sources)

---

# I Do Not Hate Containers. I Hate Docker as a Default

I do not believe containerization itself is useless.

I believe **Docker is massively overused**.

That distinction matters.

There are real cases where application containers are useful. There are environments where packaging a program together with a controlled userland is convenient. There are CI systems, large deployment fleets, ephemeral test environments, hostile dependency combinations, and distributed systems where container images can be a practical delivery mechanism.

What I reject is the modern assumption that Docker should be the default answer before anyone has even asked what the problem is.

Someone publishes a tiny web service?

Docker.

A program needs Python and three packages?

Docker.

A single-user application needs PostgreSQL?

Docker.

A developer does not want to learn how `systemd` works?

Docker.

A dependency needs to be installed?

Docker.

A README would otherwise require six commands?

Docker.

A service needs to start at boot?

Docker Compose.

A developer does not understand Linux permissions?

Put it in Docker and create an even stranger permissions problem.

This is upside-down engineering.

The first question should be:

> **What specific problem requires containerization?**

If nobody can answer that clearly, I do not want the container.

I want the software installed normally.

---

# What Docker Actually Is

Docker is not magic.

At its core, Docker is a collection of tooling around Linux isolation features, image formats, filesystems, process management, networking, and a long-running daemon.

On Linux, containers share the host kernel. Docker does not boot an independent Linux kernel for every container. It uses kernel mechanisms such as namespaces and cgroups to isolate and constrain processes.

The familiar Docker workflow adds several concepts between me and my application:

- Docker client
- Docker daemon
- images
- image layers
- registries
- tags
- manifests
- containers
- volumes
- bind mounts
- virtual networks
- port mappings
- Dockerfiles
- build contexts
- build caches
- Compose files
- container environment variables
- container health checks
- container logging
- container restart policies
- UID/GID mappings
- capabilities
- seccomp profiles
- namespaces
- cgroups
- storage drivers
- firewall rules

That can be justified when I need those things.

But if my actual goal is:

> “Run this application on this computer.”

then all of that may be unnecessary machinery.

---

# The Great Trick: Docker Does Not Remove Complexity

This is the central reason I dislike Docker.

**Docker does not eliminate complexity. It relocates it.**

The sales pitch often sounds like this:

> “Instead of installing and configuring all these dependencies, just run the container.”

That sounds wonderful until I ask what happened to the dependencies.

They did not vanish.

They are in the image.

The operating system did not vanish.

Part of one is in the image.

Networking did not vanish.

Now I have host networking and container networking.

Storage did not vanish.

Now I have host storage, container writable layers, volumes, and bind mounts.

Permissions did not vanish.

Now I have host users, container users, UID/GID mismatches, capabilities, namespace mappings, and mount permissions.

Updates did not vanish.

Now I have application updates, base image updates, image rebuilds, registry versions, image pulls, container recreation, and host updates.

Security did not vanish.

Now I have host security plus container runtime security plus image provenance plus registry security plus container configuration.

Deployment did not vanish.

Now I have Docker configuration in addition to deployment configuration.

Docker takes one stack of visible complexity and folds it into an abstraction. Then the moment something leaks through the abstraction, I have to understand **both the original system and Docker's version of it**.

That is not simplification.

That is complexity refinancing.

---

# Docker Solves Problems Most People Do Not Have

A great deal of Docker usage begins with solving hypothetical problems instead of actual ones.

A developer may say:

> “But what if the host has a different version of the library?”

Pin the dependency.

> “What if another application needs another version?”

Use the language's environment system or a package manager designed for parallel versions.

> “What if I need to reproduce the machine?”

Use declarative package management, configuration management, or a proper system configuration.

> “What if I need isolation?”

Then decide what kind of isolation is required.

> “What if I need a full separate operating environment?”

Use a system container or VM.

> “What if I just need one Python project isolated?”

Use a Python virtual environment.

> “What if I need the service to restart automatically?”

Use `systemd`, OpenRC, runit, s6, or another service manager.

> “What if I need to expose a TCP port?”

Listen on a TCP port.

The absurd part is that many people will spend hours writing Dockerfiles, Compose files, health checks, volume declarations, port mappings, and startup scripts to avoid spending fifteen minutes learning how the underlying operating system already solves the problem.

---

# The Architecture Is More Complicated Than It Needs to Be

Docker Engine uses a client-server architecture.

I type a command into the Docker CLI. The CLI talks to a daemon. The daemon manages images, networks, storage, and container lifecycle.

That is a lot of machinery for something that often boils down to:

```bash
/usr/bin/myprogram --config /etc/myprogram/config.toml
```

I generally prefer systems where the process I want to run is simply a process managed by the operating system.

For a normal Linux service, I can often understand the entire operational model with:

```text
binary
configuration
data directory
systemd unit
logs
network socket
```

With Docker, the same service may become:

```text
Docker client
Docker daemon
container runtime
image
image layers
Dockerfile
registry
Compose file
container
virtual network
port mapping
volume
bind mount
environment file
restart policy
health check
container logs
host logs
```

Docker advocates call this standardized.

I call it an unnecessary second control plane when the operating system already had one.

---

# The Daemon Model Is a Bad Default

The traditional Docker model revolves around `dockerd`, a long-running daemon.

That means Docker itself becomes infrastructure.

If the daemon has a problem, all the containers managed through it become part of that problem.

If permissions to the Docker socket are wrong, that is a problem.

If the daemon configuration conflicts with the host, that is a problem.

If networking changes made by the daemon interfere with my firewall, that is a problem.

If the daemon consumes storage through stale images, stopped containers, volumes, and cache, that is a problem.

Docker now has rootless mode, which is a genuine improvement, but the existence of rootless mode also underlines how much security attention the daemon model requires.

I prefer daemonless tools when I can get them.

That is one reason Podman has always made more conceptual sense to me.

---

# Containers Are Not Virtual Machines

This misconception refuses to die.

A Linux container is not a tiny independent computer in the same sense as a hardware virtual machine.

It generally shares the host Linux kernel.

That has consequences.

A container cannot magically supply a different kernel.

A container does not turn Linux software into Windows software.

A container does not make CPU architecture disappear.

A container built for `amd64` does not natively become an `arm64` application just because someone put it in a registry.

Docker supports multi-platform images, but that means distributing architecture-specific variants or using techniques such as emulation and cross-compilation.

Docker's own documentation warns that QEMU emulation can be much slower than native execution for compute-heavy tasks.

So when someone says:

> “Docker makes it run anywhere.”

My response is:

No.

Docker can make a **compatible userspace payload easier to deliver across compatible hosts**.

That is useful.

It is not the same thing as “runs anywhere.”

---

# The Isolation Is Not What Many People Think It Is

Containers provide useful isolation.

They are still not a magical security force field.

The host kernel remains extremely important because containers share it.

The security characteristics depend on configuration:

- user namespaces
- capabilities
- seccomp
- AppArmor or SELinux
- mounts
- devices
- networking
- privileges
- runtime configuration
- kernel security
- whether the container is rootless
- whether dangerous host paths are mounted
- whether the Docker socket is exposed

Docker's own security documentation warns that control over the Docker daemon is highly privileged. Docker also documents that a bind mount can expose host files to modification by a container.

That should destroy the simplistic idea that “it is in Docker, therefore it is safe.”

If I need a strong security boundary for untrusted code, I would much rather start by considering a **virtual machine** or another isolation technology designed around a separate kernel boundary.

Containers are process isolation.

They are not pixie dust.

---

# Docker Does Not Magically Make Software Reproducible

This is another claim that gets inflated far beyond reality.

A Dockerfile can improve reproducibility.

It does not guarantee it.

Consider:

```dockerfile
FROM ubuntu:latest
RUN apt-get update && apt-get install -y some-package
```

What exactly is reproducible about that forever?

The base image tag can change.

Package repositories can change.

Packages can be replaced.

Dependencies can change.

Remote downloads can change.

Build scripts can fetch different content.

Architecture can change behavior.

External services can change.

Time can change generated output.

Build cache can preserve old results.

A Docker image digest can lock down a specific image artifact, but that is different from saying Docker itself guarantees a fully reproducible build process.

Docker's own build cache documentation explains that some `RUN` steps can continue using cached results unless the cache is deliberately invalidated.

A cached `apt-get update` step is not a philosophical proof of reproducibility. It is a cached filesystem layer.

If I need **serious reproducibility**, I am far more interested in systems built around reproducibility as a foundational property, such as Nix or Guix.

---

# Docker Does Not Magically Make Software Portable

Docker improves one kind of portability:

**packaging a userland and application together in a standardized image format.**

That is useful.

But portability still leaks.

I still care about:

- CPU architecture
- kernel features
- device availability
- GPU drivers
- host filesystem semantics
- networking
- mounted paths
- filesystem permissions
- cgroups
- security modules
- storage drivers
- available memory
- available CPU
- DNS
- host kernel bugs
- platform-specific hardware
- architecture-specific binaries

Bind mounts can make a container strongly dependent on a particular host directory layout.

GPU workloads can depend on host drivers and runtime integration.

USB hardware can require device passthrough.

Low-level networking can require capabilities.

FUSE can require special setup.

GUI software can require display, Wayland, X11, audio, D-Bus, and device integration.

The container image is not a Star Trek transporter.

The host still exists.

---

# Docker Does Not Eliminate Dependency Problems

It packages them.

That can be valuable.

But packaging a mess is not the same as eliminating the mess.

If an application needs:

- Python
- CUDA
- FFmpeg
- a specific libc
- six Python wheels
- three system libraries
- a model runtime
- a database client

Docker can put those things in an image.

Fine.

But somebody still has to choose the versions.

Somebody still has to update them.

Somebody still has to resolve security issues.

Somebody still has to rebuild the image.

Somebody still has to understand why the application broke.

Somebody still has to keep compatibility with the host where the abstraction leaks.

Dependency management has not disappeared.

It has become image maintenance.

---

# Docker Networking Is Often Self-Inflicted Complexity

Networking is one of the clearest examples of Docker making simple things strange.

Without Docker:

```text
program -> host network -> port 8080
```

With Docker:

```text
program
  -> container network namespace
  -> container interface
  -> bridge
  -> NAT
  -> Docker firewall rules
  -> published host port
  -> host network
```

Again, there are reasons for doing this.

But when the application only needed to listen on one port, I do not automatically see this as progress.

Docker also creates firewall rules on Linux for bridge networking and port publishing. Docker's documentation explains that it creates its own chains and NAT rules, and that disabling Docker's firewall management is not generally recommended because it can break container networking.

Docker's own documentation also explains interactions with `ufw` where published container traffic can bypass the path users may expect from normal host firewall rules.

This is exactly the kind of abstraction leak I hate.

The container system is supposed to simplify deployment.

Instead I may need to understand:

- iptables
- nftables
- NAT
- forwarding
- bridge interfaces
- Docker chains
- published ports
- container DNS
- network namespaces
- host firewalls

I already had networking.

Docker gave me more networking.

---

# Docker Storage Turns Ordinary Files Into Infrastructure

Persistent data is another area where container simplicity becomes strangely ceremonial.

A normal application might store data here:

```text
/var/lib/myapp
```

Back it up.

Restore it.

Inspect it.

Done.

Docker introduces choices:

- writable container layer
- named volume
- anonymous volume
- bind mount
- tmpfs mount

Then I need to understand lifecycle semantics.

Does the data survive container deletion?

Where is the volume physically stored?

Can I inspect it easily from the host?

Does my backup system see it?

Is the volume still attached?

Did Compose create another one?

Did I accidentally create an anonymous volume?

Did a mount obscure files already present in the image?

Docker's documentation explicitly notes that mounting over existing container data can obscure it and that recreating the container without the mount may be the practical way to reveal it again.

This is not impossible.

It is simply more conceptual machinery than a lot of applications need.

---

# Permissions Become Needlessly Weird

Unix permissions are already capable of being confusing.

Docker adds another coordinate system.

I now have:

- host UID
- host GID
- container UID
- container GID
- image-defined users
- volume ownership
- bind-mount ownership
- namespace mappings
- rootless mappings
- capabilities
- SELinux labels
- AppArmor profiles

The application writes a file.

Who owns it?

The host user?

Container root?

UID 1000 inside the container?

UID 1000 on the host?

A remapped UID?

A service account?

Then developers start putting `chmod 777` into instructions because the abstraction they were told would simplify deployment has made file ownership opaque.

That is not a Docker-only failure, but Docker makes this class of mistake extremely common.

---

# Docker Creates Its Own Disk-Space Problems

Images are layered.

Layers can be shared.

That is clever.

It can also become a landfill.

Docker has:

- images
- intermediate layers
- dangling images
- stopped containers
- volumes
- build cache
- registry cache
- multi-platform image variants

Docker's own documentation has an entire section on pruning unused objects because Docker deliberately takes a conservative approach to removing them.

In other words:

> Docker creates a significant category of storage state that I periodically have to ask Docker to clean up.

Yes, ordinary package managers also use caches.

But Docker can accumulate gigabytes or tens of gigabytes of image data surprisingly quickly, especially on development machines that build frequently.

The classic ritual appears:

```bash
docker system prune
```

That command exists because the abstraction has garbage collection as an operational concern.

---

# The Build Cache Is Another Thing I Should Not Have to Babysit

Docker's build cache can save enormous amounts of time.

It can also create another layer of mental state between the source code and the result.

A Dockerfile instruction may reuse a cached layer.

Changing one layer may invalidate everything downstream.

Some commands may use cached results even though remote package repositories have changed.

Then developers learn special Dockerfile choreography:

- copy dependency manifests first
- install dependencies
- copy source later
- use multi-stage builds
- mount caches
- choose cache backends
- prune cache
- disable cache
- force cache busting

This is clever engineering inside Docker's model.

My complaint is that the model itself became necessary because I wrapped the build in Docker.

I should not need to become a Docker cache whisperer to compile a program.

---

# Resource Isolation Is Not Automatic

People frequently talk about containers as though they automatically put applications into tidy resource boxes.

Docker's own documentation says that **containers have no CPU or memory constraints by default**.

A container can consume as much as the host scheduler allows unless limits are configured.

So “it is containerized” does not mean:

- memory capped
- CPU capped
- I/O capped
- safe from starving the host

Those protections require configuration.

Again, Docker can do the job.

My issue is the assumption that the job is automatically done merely because a process has been wrapped in Docker.

---

# Docker Desktop Is Especially Absurd for Many Desktop Users

Docker on a Linux server at least maps naturally onto Linux container primitives.

Docker Desktop on macOS and Windows has to bridge a larger gap because Linux containers need Linux.

Docker Desktop therefore uses a Linux environment or VM layer for Linux container workloads.

That means someone may install Docker because they think containers are “lighter than VMs,” only to run Linux containers through a VM-backed desktop product.

This can still be useful.

But it is wonderfully circular.

The user wants to avoid setting up software directly.

So they install Docker Desktop.

Docker Desktop runs a Linux VM environment.

The container runs inside that environment.

Host files are shared into it.

Ports are forwarded back out.

Now we have a Russian nesting doll of abstraction so that `postgres` can listen on port 5432.

For some development teams that consistency is worth it.

For an individual user running a few local services, I often find it ridiculous.

---

# Hardware Access Is Where the Illusion Starts Falling Apart

Docker looks cleanest when an application only needs:

- CPU
- memory
- files
- TCP

The moment software interacts deeply with hardware, reality starts clawing through the container wall.

Examples:

- GPUs
- NPUs
- USB devices
- serial devices
- webcams
- audio devices
- SDR hardware
- Bluetooth
- FUSE
- kernel modules
- privileged networking
- direct rendering
- Wayland
- X11
- D-Bus
- special `/dev` nodes

Now I may need:

```text
--device
--privileged
--group-add
--cap-add
bind mounts
runtime integrations
host driver compatibility
environment variables
special security profiles
```

At some point the container is no longer providing clean isolation.

It is a squid wrapped around the host with tentacles passed through every boundary.

For low-level AI acceleration, embedded hardware, edge devices, GPU stacks, and custom kernel work, I often prefer to understand and configure the actual machine.

---

# Stateful Services Expose the Contradictions

Docker culture loves the phrase “containers are disposable.”

Databases are not disposable.

User data is not disposable.

Model repositories are not disposable.

Configuration may not be disposable.

Queues may not be disposable.

Secrets are not disposable.

Indexes may not be disposable.

So the moment I put a stateful application in Docker, I have to carefully externalize the state.

Now the supposedly self-contained container depends on:

- volumes
- backup procedures
- secrets
- external configuration
- networking
- persistent database storage
- restore procedures

The container becomes disposable only because all the important parts are deliberately placed somewhere else.

That can be a sound architecture.

It is not automatically a simpler architecture.

---

# Debugging Gets Worse

Native service:

```bash
ps
journalctl
systemctl status
ss -lntp
lsof
strace
gdb
cat /etc/myapp/config
```

Containerized service:

```bash
docker ps
docker logs
docker inspect
docker exec
docker stats
docker network inspect
docker volume inspect
docker compose ps
docker compose logs
```

And then, if the problem crosses the boundary, I still use:

```bash
ps
journalctl
ss
iptables
nft
lsof
strace
```

So Docker does not replace the underlying diagnostic knowledge.

It adds another diagnostic layer.

The worst Docker failures are often exactly the ones where I need to understand both worlds simultaneously.

---

# Observability Gets More Complicated

A normal service can log to the journal, syslog, a file, or an application-specific backend.

Docker introduces logging drivers and container log handling.

Then production systems often add:

- log shippers
- collectors
- metrics exporters
- tracing
- sidecars
- dashboards

This can be excellent at scale.

It is nonsense for a two-service personal machine.

I do not need an observability architecture to discover why a Python script exited.

Sometimes `journalctl -u myservice` is enough.

---

# Docker Encourages Disposable Knowledge

This may be the thing I dislike most culturally.

Docker can encourage people to stop understanding the host.

Instead of learning:

- how services start
- where configuration lives
- how DNS works
- how ports work
- how Unix permissions work
- how package managers work
- how system logs work
- how processes work
- how signals work
- how the filesystem hierarchy works

the developer learns:

```bash
docker compose up -d
```

Then when it works, they feel productive.

When it breaks, they search for another incantation.

That is not mastery.

It is button-memory.

I would rather understand a smaller number of durable operating-system concepts than memorize an expanding vocabulary of Docker-specific workarounds.

---

# The Dockerfile Is Not the Beautiful Abstraction People Pretend It Is

A Dockerfile looks simple.

That simplicity is deceptive.

Soon I am thinking about:

- base image choice
- image provenance
- tags versus digests
- layer ordering
- package cache cleanup
- multi-stage builds
- build arguments
- secrets
- cache mounts
- architecture
- user creation
- UID mapping
- entrypoints
- commands
- signal handling
- PID 1 behavior
- health checks
- file ownership
- file permissions
- copying build artifacts
- image size
- package pinning

Then somebody says:

> “But look how easy deployment is. It is one Dockerfile.”

No.

The complexity is **inside the Dockerfile**.

---

# Compose Is Convenient Until It Becomes Its Own Operating System

Docker Compose is genuinely convenient.

It is also one of the easiest ways to turn a small stack into a miniature platform.

A simple Compose file can describe:

- services
- images
- builds
- ports
- networks
- volumes
- secrets
- environment variables
- dependencies
- health checks
- restart policies
- resource settings
- profiles

At some scale that is helpful.

But I have seen tiny projects use a wall of YAML to describe what could have been:

```text
one service
one database
one config file
two systemd units
```

Compose becomes appealing because it hides the system.

That is precisely why I distrust using it by default.

---

# Image Registries Create Another Supply Chain

Docker culture normalizes pulling executable filesystem images from registries.

That creates another trust chain:

```text
developer
-> Dockerfile
-> base image
-> registry
-> image tag
-> image maintainer
-> distro packages
-> language packages
-> build scripts
-> final container
```

This is not necessarily worse than every other software distribution mechanism.

But it is not magically safer.

Tags can move.

Images can become stale.

Public images can be malicious.

Dependencies inside images still need patching.

Secrets can accidentally end up in build layers.

Users can pull something because a README told them to run it and never inspect what they are actually executing.

Docker made software distribution easier.

Ease of distribution is also ease of distributing bad software.

---

# Docker Makes Simple Software Look Like Distributed Infrastructure

One of my favorite anti-patterns is a local application with:

- frontend container
- backend container
- database container
- Redis container
- reverse proxy container
- worker container

for a service with one user.

Technically, this can be tidy.

Operationally, it is a six-headed hydra where a perfectly good monolith would have been happier.

Every service now has:

- lifecycle
- networking
- logs
- configuration
- versioning
- dependency order
- health checks

The user wanted an application.

They got a tiny data center.

---

# The Microservice Cargo Cult Made Everything Worse

Docker became culturally fused with microservices.

That combination produced enormous amounts of accidental architecture.

Large organizations sometimes need independently deployable services.

Small projects often do not.

Splitting software into many pieces because Netflix did it is like buying an airport baggage system because I own three suitcases.

Docker lowered the friction of creating services.

That also lowered the friction of creating **too many services**.

Architecture should be driven by boundaries that matter:

- scaling
- ownership
- security
- failure domains
- deployment cadence
- data boundaries

Not by how satisfying it feels to type:

```bash
docker compose up
```

---

# Why I Think Developer Laziness Drives So Much of Docker Culture

This is the part people will hate.

I think a substantial portion of Docker's popularity comes from **developer laziness**.

Not every Docker user is lazy.

Not every use of Docker is lazy.

There are extremely skilled engineers using Docker for good reasons.

But the cultural default absolutely rewards a low-effort mindset.

A developer can avoid learning the target operating system.

They can avoid learning package management.

They can avoid learning service management.

They can avoid learning deployment.

They can avoid understanding permissions.

They can avoid documenting a clean native installation.

They can avoid supporting normal distro packages.

They can avoid understanding which dependencies are actually required.

They can write:

```text
Install Docker.
Run this Compose file.
```

and transfer the integration burden to the user's machine.

From the developer's perspective, that is wonderfully convenient.

From my perspective, it often means:

> “I did not want to properly package my software, so please install an entire container platform.”

That is not always engineering elegance.

Sometimes it is outsourced effort.

---

# Convenience Is Not the Same Thing as Engineering

Convenience is good.

I like convenient tools.

But convenience should not become the highest engineering value.

The lowest-effort path for the developer can become the highest-complexity path for every user.

Suppose ten thousand users each install Docker to run one application because the project did not want to provide:

- a `.deb`
- an RPM
- an Arch package
- a proper install script
- a binary release
- a Python package
- a systemd unit

The project saved effort.

Ten thousand machines gained a container runtime.

I do not call that globally efficient.

I call it shifting work downstream.

---

# What I Would Do Instead for Most Software

My default stack is boring.

That is a compliment.

For a normal Linux service I prefer:

1. Install the required runtime.
2. Install dependencies using the native package manager or language tooling.
3. Put configuration in a normal location.
4. Put data in a normal location.
5. Create a dedicated service user if needed.
6. Create a `systemd` unit.
7. Use the host firewall normally.
8. Use the host logging system.
9. Back up the actual data directory.
10. Update the application normally.

Example:

```text
/usr/local/bin/myapp
/etc/myapp/config.toml
/var/lib/myapp/
/var/log/myapp/     # if the app needs files rather than journald
/etc/systemd/system/myapp.service
```

I understand this.

Linux understands this.

Backup software understands this.

Monitoring software understands this.

Other administrators understand this.

There is no imaginary shipping container sitting between me and my process.

---

# Better Alternatives When I Really Do Need Isolation

If I genuinely need what Docker provides, I still have choices.

The correct alternative depends on the actual requirement.

---

# Podman

If I want OCI application containers but dislike Docker's traditional daemon-centric model, **Podman** is one of the first alternatives I consider.

Podman supports:

- OCI containers
- Docker-compatible workflows
- rootless operation
- daemonless container execution
- pods
- Docker Compose compatibility in many workflows
- Kubernetes-oriented workflows

For many users, Podman offers the part of Docker they actually wanted without making a privileged central Docker daemon the center of the machine.

I still would not use Podman when I do not need containers.

That is important.

Replacing unnecessary Docker with unnecessary Podman is not victory.

It is repainting the container.

---

# containerd and nerdctl

Docker itself relies on lower-level container technology.

If I am working close to Kubernetes or want a more direct relationship with `containerd`, **nerdctl** provides a Docker-like CLI for containerd.

It supports features such as:

- Docker-like command syntax
- Compose
- rootless operation
- OCI images
- image signing workflows
- advanced image distribution features

This is especially interesting when containerd is already part of the environment.

Again, it is not something I would install to run a trivial application that could have been a normal service.

---

# Incus and LXC

If what I really want is something that behaves more like a **small Linux machine**, application containers may be the wrong abstraction.

LXC provides system containers.

Incus provides a higher-level management system for system containers and virtual machines, and newer Incus versions can also work with OCI application images.

A system container can feel much closer to a conventional Linux installation:

```text
boot environment
package manager
multiple services
normal filesystem hierarchy
normal users
normal service management
```

That can be vastly more intuitive when my goal is:

> “Give this workload its own Linux environment.”

rather than:

> “Wrap each process in an application image.”

If I need several separate Linux environments on one machine, Incus is often more conceptually honest than pretending every application is a disposable microservice.

---

# systemd-nspawn

On a Linux machine already using systemd, **systemd-nspawn** is another elegant option.

It can run a command or boot a lightweight Linux userspace inside a namespace container.

I like the conceptual fit.

Instead of bolting an enormous parallel management culture onto Linux, it extends tooling that is already part of many Linux systems.

It is especially appealing for:

- test environments
- throwaway distributions
- system containers
- isolated build environments
- lightweight OS trees

It is not as fashionable.

That may be one of its virtues.

---

# Nix and NixOS

If my real problem is **reproducibility**, Docker is not necessarily the best answer.

Nix attacks the dependency problem much closer to its root.

Nix emphasizes:

- declarative environments
- isolated builds
- precise dependency graphs
- multiple package versions
- reproducible environments
- rollbacks
- system configuration as code

NixOS goes further and makes much of the operating system declarative.

If I want to describe a machine and reproduce it, that is conceptually much closer to what I actually mean than hiding a filesystem snapshot inside a container image.

Nix has its own learning curve.

It can be weird.

Its language and ecosystem can frustrate people.

But at least the weirdness is aimed directly at **declarative package and system management**, rather than treating a filesystem image as the answer to everything.

---

# GNU Guix

GNU Guix attacks many of the same problems from a different philosophical and technical direction.

It provides:

- functional package management
- isolated environments
- declarative system configuration
- reproducible software deployment
- multiple versions
- rollbacks

For people who care about software freedom and reproducibility, Guix can be a much more principled solution than “ship a Docker image.”

---

# Virtual Machines

If I need strong isolation, a different kernel, or an entire independent operating system, I use a **virtual machine**.

Examples include:

- KVM/QEMU
- libvirt
- Proxmox
- VMware
- Hyper-V
- VirtualBox in some desktop scenarios

VMs cost more resources than containers.

They also provide a conceptually cleaner boundary.

The guest has its own kernel.

I can snapshot the whole machine.

I can run another operating system.

I can isolate risky software more strongly.

Sometimes the “heavier” solution is the better solution because it actually matches the requirement.

I would rather use an honest VM than abuse containers until they imitate one.

---

# Language-Native Environments

A huge percentage of Docker usage can be replaced by tools that already belong to the language ecosystem.

## Python

Use:

- `venv`
- `uv`
- `pipx`
- Poetry
- PDM
- Conda or Mamba where appropriate

Python's standard `venv` creates lightweight isolated Python environments without requiring an entire container runtime.

For a Python application, that is frequently all I need.

## Node.js

Use:

- `npm`
- `pnpm`
- Yarn
- `nvm`
- `fnm`
- Volta

## Rust

Use:

- `cargo`
- `rustup`

## Go

Use:

- Go modules
- static or mostly self-contained binaries where practical

## Ruby

Use:

- Bundler
- rbenv
- RVM where appropriate

## Java

Use:

- Maven
- Gradle
- SDKMAN
- jlink for custom runtime images when appropriate

The language ecosystem usually knows more about the language's dependency model than Docker does.

Docker should not be my first dependency manager.

---

# Traditional Package Managers

Linux already has mature software distribution systems.

Use them.

Examples:

```text
apt / dpkg
dnf / rpm
zypper
pacman
apk
xbps
Portage
```

Package managers provide:

- dependency resolution
- upgrades
- removal
- file ownership tracking
- signatures
- integration with the operating system
- shared libraries
- security updates

Developers often complain that native packaging is work.

Yes.

Packaging software properly is work.

That does not mean making every user install Docker is better.

---

# Configuration Management

If my real problem is:

> “I need ten machines configured the same way.”

then I may need configuration management, not containers.

Options include:

- Ansible
- Salt
- Puppet
- Chef
- NixOS
- Guix System

These tools let me describe the actual machine.

That is often exactly what I want.

---

# Flatpak and Similar Desktop Packaging

For desktop applications that need dependency isolation and sandboxing, **Flatpak** may make much more sense than Docker.

Desktop packaging has different needs:

- GUI integration
- audio
- portals
- themes
- display servers
- desktop permissions
- user data
- application launchers

Docker is not naturally designed as a desktop application distribution system.

Use a tool designed for the environment.

Other cases may call for:

- AppImage
- Snap
- native distro packages

I would choose based on the actual target, not on Docker's popularity.

---

# When Docker Actually Makes Sense

Despite everything above, there are situations where I would willingly use Docker.

## 1. Ephemeral CI environments

If every CI job needs a clean disposable environment, containers are excellent.

## 2. Integration testing

If I need temporary instances of:

- PostgreSQL
- Redis
- RabbitMQ
- Elasticsearch
- another service

for automated tests, containers can be extremely convenient.

## 3. Large distributed deployments

If an organization already has an OCI-based deployment platform, building OCI images may be the correct delivery mechanism.

## 4. Kubernetes environments

If the production target is Kubernetes, using OCI images is part of the architecture.

Docker itself is not required for Kubernetes, but container images obviously matter.

## 5. Complex third-party software with ugly dependencies

If a vendor application is miserable to install and the container is well maintained, the container may be the least terrible option.

## 6. Repeated disposable labs

Security labs, training environments, testing environments, and experimental stacks can benefit from quick reset and recreation.

## 7. Cross-team development stacks

A team may reasonably choose containers so twenty developers can instantiate the same multi-service environment quickly.

## 8. Packaging a service for unknown Linux hosts

If I genuinely do not control the target distribution and need a consistent OCI-based delivery mechanism, a container image can be useful.

These are real use cases.

They are not permission to Dockerize the universe.

---

# When I Would Absolutely Not Use Docker

I would avoid Docker by default for:

- a single local application
- a tiny personal server
- a Raspberry Pi running a few services
- a program with straightforward dependencies
- a Python project that works in `venv`
- a Go or Rust program that ships as a clean binary
- a system service that only needs a config file and a port
- hardware-heavy software requiring lots of device access
- low-level AI accelerator work
- custom kernel development
- applications tightly integrated with the desktop
- simple databases on a machine I fully control
- software I am actively trying to understand deeply
- anything where Docker adds more moving parts than the application itself

On small systems, Docker overhead is not only CPU and RAM.

The larger overhead is **conceptual**.

That matters.

---

# My Decision Rule

Before I use Docker, I ask:

## What problem am I solving?

If the answer is:

> “Everybody uses Docker.”

I do not use Docker.

If the answer is:

> “The README uses Docker.”

That is not enough.

If the answer is:

> “It is easier.”

Easier for whom?

If the answer is:

> “Dependencies.”

I ask whether native package or language isolation solves it.

If the answer is:

> “Reproducibility.”

I consider Nix, Guix, lockfiles, and declarative configuration.

If the answer is:

> “Isolation.”

I decide whether I need:

- process isolation
- application container
- system container
- sandbox
- virtual machine

If the answer is:

> “Deployment.”

I ask what the actual deployment target is.

Only after that do I choose a tool.

Tools are supposed to follow requirements.

Docker culture often reverses that.

---

# My Preferred Deployment Philosophy

I prefer systems that are:

## 1. Understandable

I should be able to explain where the program is, where its data is, how it starts, what port it uses, and how to stop it.

## 2. Native

If the operating system already has a good mechanism, I use it.

## 3. Inspectable

I want ordinary tools to work.

## 4. Boring

Boring infrastructure is beautiful.

A service that has run for four years because `systemd` starts it and `apt` updates its libraries is a success.

## 5. Minimal

Every additional abstraction should justify itself.

## 6. Replaceable

I do not want an application unnecessarily coupled to a giant platform.

## 7. Explicit

I would rather understand the dependency than hide it in an image.

## 8. Appropriate to scale

A Raspberry Pi is not AWS.

My laptop is not Netflix.

A personal web app is not a hyperscale microservice fleet.

Architecture should match reality.

---

# Why “Just Install It” Is Usually Better

There is something strangely unfashionable about simply installing software.

I think we should bring that back.

If I install software normally:

- the files are where the OS expects them
- service management works normally
- logs work normally
- networking works normally
- firewall rules work normally
- updates work normally
- backup tools see the data normally
- process tools see the process normally
- disk tools see the files normally
- hardware access works normally

The machine remains a machine.

It does not become a host for a second pretend operating layer.

For most single-user systems and small servers, that is exactly what I want.

---

# “But Docker Makes Cleanup Easy”

Sometimes.

A container can be deleted.

An image can be deleted.

That is convenient.

But good package managers also know which files belong to packages.

Virtual environments can be deleted.

Nix profiles can be removed or rolled back.

A dedicated application directory can be removed.

A VM can be deleted.

A system container can be deleted.

Docker does not own the concept of cleanup.

---

# “But Docker Prevents Dependency Conflicts”

Sometimes.

So do:

- virtual environments
- Nix
- Guix
- language version managers
- static linking
- dedicated service users
- system containers
- VMs

The existence of dependency conflicts does not uniquely imply Docker.

---

# “But Docker Makes Onboarding Easy”

It can.

This is probably one of Docker's strongest arguments.

A new developer can clone a repository and run:

```bash
docker compose up
```

That is convenient.

But I ask what happens next.

Does the developer understand the stack?

Can they debug it?

Do they know where the database lives?

Do they know why a port failed?

Do they know what owns the generated files?

Do they know why the container cannot access the GPU?

Do they know how networking is routed?

Fast onboarding can become delayed understanding.

That trade can be worth it.

It should still be recognized as a trade.

---

# “But Docker Is Industry Standard”

So what?

Industry standards can be useful.

They can also become cargo cults.

The fact that a technology is common proves that it has ecosystem momentum.

It does not prove it is appropriate for every machine.

Git is common.

I do not put every file on my computer in Git.

PostgreSQL is excellent.

I do not use PostgreSQL for every data structure.

Kubernetes is powerful.

I do not run Kubernetes to host a static HTML page.

Docker is useful.

I do not therefore containerize every process.

---

# “But Containers Are Lightweight”

Compared with full virtual machines, often yes.

Compared with **not using a container at all**, no.

The lightest wrapper around a process is no wrapper.

The simplest network namespace is the host network namespace.

The simplest volume is a directory.

The simplest port mapping is no port mapping.

The simplest image is a package or binary.

The simplest container runtime is no container runtime.

This sounds obvious because it is obvious.

Docker culture somehow trained people to forget it.

---

# Poor Design, in My View

When I call Docker poorly designed, I am not claiming every Docker engineer writes bad code.

That would be silly.

Docker contains a huge amount of sophisticated engineering.

My criticism is about the **overall product architecture and the defaults it normalized**.

I dislike:

- the daemon-centric heritage
- the amount of hidden mutable state
- the network manipulation
- the storage abstraction
- the image-layer lifecycle
- the volume lifecycle
- the permissions friction
- the build-cache behavior
- the registry-centric workflow
- the tendency to make implementation details into deployment contracts
- the tendency to require a container platform for applications that do not require containers

A product can contain brilliant code and still create an ecosystem I consider poorly designed.

Those are not contradictory statements.

---

# Poorly Written, in the Sense That Matters to Me

I also sometimes call Docker “poorly written,” but I mean something broader than source-code aesthetics.

From my perspective, software is poorly written when the user repeatedly has to absorb implementation complexity that should have stayed behind the interface.

Examples:

- firewall behavior leaking into ordinary application deployment
- UID/GID confusion leaking through bind mounts
- hidden layers consuming large amounts of disk
- cache invalidation becoming a development discipline
- Desktop VM behavior affecting filesystem performance
- networking abstractions requiring host firewall knowledge
- container lifecycle becoming entangled with persistent storage lifecycle

An abstraction is supposed to compress complexity.

When it repeatedly makes me understand both sides of the abstraction, I consider that abstraction unsuccessful for the use case.

---

# Docker Is Often a Solution Looking for a Problem

This is my ultimate complaint.

Docker became a badge of modernity.

Projects began adding Docker support not because they needed containers, but because projects were expected to have Docker support.

Then developers began using Docker because the project had Docker support.

Then users began expecting Docker because developers used Docker.

The loop fed itself.

Eventually:

```text
Docker is common because Docker is common.
```

That is ecosystem inertia.

It is not an engineering argument.

---

# The Better Default

My preferred default is:

```text
Can I install it normally?
    |
    +-- Yes -> Install it normally.
    |
    +-- No -> Why not?
               |
               +-- Language dependency conflict -> use language environment
               |
               +-- Need reproducible environment -> consider Nix/Guix
               |
               +-- Need isolated Linux userspace -> consider Incus/LXC/nspawn
               |
               +-- Need strong OS boundary -> use a VM
               |
               +-- Need OCI deployment -> Podman/containerd/Docker are candidates
               |
               +-- Need Kubernetes -> build OCI images
```

Docker comes **after** the requirement.

Not before it.

---

# The Bottom Line

I hate Docker because it turned a specialized technique into a reflex.

It convinced developers that:

- packaging is deployment
- containers are portability
- image snapshots are reproducibility
- namespaces are virtual machines
- isolation is automatic security
- adding a platform is simplification
- YAML is architecture
- hiding dependencies is the same as managing dependencies
- moving complexity is the same as removing complexity

I reject all of that.

Docker is a tool.

A useful tool.

A technically impressive tool.

And, in my view, one of the most over-applied tools in modern software.

I do not want every application in a container.

I do not want every personal server to become a miniature cloud.

I do not want every developer to treat the operating system as an embarrassing implementation detail.

I do not want to install a daemon, image store, virtual networking system, storage abstraction, registry workflow, and YAML orchestration layer just because somebody could not be bothered to package their software properly.

For most software on machines I control, I would rather:

> **install the program, configure the program, run the program, and understand the program.**

If I need isolation, I will choose isolation deliberately.

If I need reproducibility, I will choose a tool designed around reproducibility.

If I need a separate Linux environment, I will use a system container.

If I need a separate kernel or strong machine boundary, I will use a VM.

If I need OCI application containers, I may use Podman, containerd, nerdctl, or even Docker.

But Docker does not get to be the default simply because half the industry forgot that Linux already knows how to run a process.

That is my line in the sand.

**Use Docker when Docker solves a real problem.**

**Otherwise, just install the damn software.**

---

# Technical Notes and Sources

This essay is intentionally opinionated, but the technical claims are grounded in the current documentation of the technologies discussed.

## Docker

- Docker Engine architecture and daemon model:  
  <https://docs.docker.com/engine/>

- Docker overview and client-server architecture:  
  <https://docs.docker.com/get-started/docker-overview/>

- Docker Engine security and daemon attack surface:  
  <https://docs.docker.com/engine/security/>

- Docker rootless mode:  
  <https://docs.docker.com/engine/security/rootless/>

- Docker bind mounts and host filesystem implications:  
  <https://docs.docker.com/engine/storage/bind-mounts/>

- Docker volumes and writable-layer performance:  
  <https://docs.docker.com/engine/storage/volumes/>

- Docker resource constraints. Containers are unconstrained by default unless configured otherwise:  
  <https://docs.docker.com/engine/containers/resource_constraints/>

- Docker firewall and packet filtering behavior:  
  <https://docs.docker.com/engine/network/packet-filtering-firewalls/>

- Docker iptables behavior:  
  <https://docs.docker.com/engine/network/firewall-iptables/>

- Docker build cache behavior:  
  <https://docs.docker.com/build/cache/>

- Docker cache invalidation rules:  
  <https://docs.docker.com/build/cache/invalidation/>

- Docker object pruning and disk cleanup:  
  <https://docs.docker.com/engine/manage-resources/pruning/>

- Docker multi-platform builds and QEMU emulation notes:  
  <https://docs.docker.com/build/building/multi-platform/>

- Docker Desktop documentation:  
  <https://docs.docker.com/desktop/>

## Podman

- Podman project:  
  <https://podman.io/>

Podman describes itself as daemonless, supports rootless containers, and works with OCI container formats.

## containerd and nerdctl

- nerdctl project:  
  <https://github.com/containerd/nerdctl>

nerdctl provides a Docker-compatible CLI for containerd and supports Compose and optional rootless operation.

## LXC and Incus

- LXC:  
  <https://linuxcontainers.org/lxc/>

- Incus:  
  <https://linuxcontainers.org/incus/>

LXC focuses on Linux system containers. Incus manages system containers and virtual machines and also supports OCI application-container workflows.

## systemd-nspawn

- systemd-nspawn manual:  
  <https://www.freedesktop.org/software/systemd/man/latest/systemd-nspawn.html>

`systemd-nspawn` can run commands or boot Linux OS trees inside lightweight namespace containers.

## Nix and NixOS

- Nix and NixOS:  
  <https://nixos.org/>

Nix emphasizes declarative, isolated, reproducible package and environment management.

## GNU Guix

- GNU Guix:  
  <https://guix.gnu.org/>

Guix provides functional package management, isolated environments, rollbacks, and declarative system configuration.

## Python Virtual Environments

- Python `venv`:  
  <https://docs.python.org/3/library/venv.html>

Python's standard library provides lightweight virtual environments with isolated package sets.

---

## Final Principle

> **Abstraction should be earned.**
>
> Every layer I add should solve a problem I actually have.
>
> If the host operating system can already run the software cleanly, predictably, securely, and maintainably, then I see no virtue in adding a container merely because containers are fashionable.

