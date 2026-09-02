// Pure JS URL decomposition — the QML-owned mirror of
// target.cpp makeUrlTarget (design canon #3): the footer renders any
// flat URL; C++ stops pre-decomposing for the UI.
//
//   scheme    = "<scheme>://"
//   hostOrDir = host[:port]  (port only when non-default)
//   middle    = path-left of the last '/'
//   tail      = "/" + last path segment + query + fragment
//
// QUrl parity notes (verified against a C++ QUrl probe):
//   - host is lowercased, userinfo stripped, port dropped when it
//     equals the scheme default (80/https 443)
//   - path/query/fragment are percent-decoded (UTF-8), '+' is NOT a
//     space, invalid percent sequences stay raw
//   - query is appended only when non-empty; fragment whenever a '#'
//     is present (even bare "#")
.pragma library

function _decode(s) {
    // decodeURIComponent throws on malformed input — QUrl keeps such
    // sequences raw instead, so fall back to the undecoded text.
    try {
        return decodeURIComponent(s)
    } catch (e) {
        return s
    }
}

function decompose(urlString) {
    const s = String(urlString)
    if (s === "")
        return { scheme: "", hostOrDir: "", middle: "", tail: "" }

    let rest = s
    let scheme = ""
    const schemeEnd = rest.indexOf("://")
    if (schemeEnd > 0) {
        scheme = rest.slice(0, schemeEnd).toLowerCase() + "://"
        rest = rest.slice(schemeEnd + 3)
    }

    // authority ends at the first '/', '?' or '#'
    let authorityEnd = rest.length
    for (let i = 0; i < rest.length; i++) {
        const c = rest.charAt(i)
        if (c === "/" || c === "?" || c === "#") {
            authorityEnd = i
            break
        }
    }
    const authority = rest.slice(0, authorityEnd)
    const remainder = rest.slice(authorityEnd)

    // userinfo (up to the last '@') is not rendered
    let hostPart = authority
    const at = hostPart.lastIndexOf("@")
    if (at >= 0)
        hostPart = hostPart.slice(at + 1)

    // port: after ']' for [v6] hosts, else after the last ':'
    let host = hostPart
    let port = -1
    if (hostPart.indexOf("[") === 0) {
        const close = hostPart.indexOf("]")
        if (close >= 0) {
            host = hostPart.slice(1, close)
            const colon = hostPart.indexOf(":", close)
            if (colon >= 0 && /^\d+$/.test(hostPart.slice(colon + 1)))
                port = parseInt(hostPart.slice(colon + 1), 10)
        }
    } else {
        const colon = hostPart.lastIndexOf(":")
        if (colon >= 0 && /^\d+$/.test(hostPart.slice(colon + 1))) {
            host = hostPart.slice(0, colon)
            port = parseInt(hostPart.slice(colon + 1), 10)
        }
    }
    host = host.toLowerCase()
    if (port !== -1) {
        const defaultPort = scheme === "https://" ? 443 : 80
        if (port !== defaultPort)
            host += ":" + port
    }

    // fragment first ('#' has precedence for query splitting)
    let hashPart = null
    let queryPart = null
    let body = remainder
    const hash = body.indexOf("#")
    if (hash >= 0) {
        hashPart = body.slice(hash + 1)
        body = body.slice(0, hash)
    }
    const question = body.indexOf("?")
    if (question >= 0) {
        queryPart = body.slice(question + 1)
        body = body.slice(0, question)
    }

    const path = _decode(body)
    const lastSlash = path.lastIndexOf("/")
    const middle = lastSlash > 0 ? path.slice(0, lastSlash) : ""
    let tail = path.length === 0 ? "" : "/" + path.slice(lastSlash + 1)
    if (queryPart !== null) {
        const q = _decode(queryPart)
        if (q !== "")
            tail += "?" + q
    }
    if (hashPart !== null)
        tail += "#" + _decode(hashPart)

    return { scheme: scheme, hostOrDir: host, middle: middle, tail: tail }
}
