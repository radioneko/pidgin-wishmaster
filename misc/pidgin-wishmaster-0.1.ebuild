# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-plugins/pidgin-libnotify/pidgin-libnotify-0.14.ebuild,v 1.7 2010/07/22 20:19:32 pva Exp $

EAPI="2"

inherit eutils

DESCRIPTION="pidgin-wishmaster provides better nick completion for MUC"
HOMEPAGE="http://radioanon.ru/"
SRC_URI="http://radioanon.ru/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 ppc x86"
IUSE="debug"

RDEPEND="net-im/pidgin[gtk]
	>=x11-libs/gtk+-2"

DEPEND="${RDEPEND}
	dev-util/pkgconfig"

src_configure() {
	econf \
		--disable-static \
		$(use_enable debug)
}

src_install() {
	emake install DESTDIR="${D}" || die "make install failed"
	find "${D}" -name '*.la' -delete
	dodoc AUTHORS ChangeLog INSTALL NEWS README VERSION || die
}
