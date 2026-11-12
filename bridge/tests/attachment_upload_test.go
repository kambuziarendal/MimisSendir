package signalmeow

import (
    "bytes"
    "net/url"
    "strconv"
    "testing"
)

// No Signal credentials, sockets or account state. Exercise the real encryption
// output and the exact upload URL function used by UploadAttachment.
func TestAttachmentUploadUsesEncryptedLength(t *testing.T) {
    for _, plainSize := range []int{0, 1, 15, 16, 17, 541, 1024, 1048576} {
        payload, err := macAndAESEncrypt(bytes.Repeat([]byte{7}, 64), bytes.Repeat([]byte{3}, plainSize))
        if err != nil { t.Fatal(err) }
        request, err := url.Parse(attachmentUploadPath(payload))
        if err != nil { t.Fatal(err) }
        length, err := strconv.Atoi(request.Query().Get("uploadLength"))
        if err != nil || length != len(payload) || length <= plainSize {
            t.Fatalf("invalid upload contract: plain=%d encrypted=%d query=%q", plainSize, len(payload), request.RawQuery)
        }
        if request.Path != "/v4/attachments/form/upload" { t.Fatal(request.Path) }
        if len(request.Query()) != 1 { t.Fatal("unexpected request parameters") }
    }
}
