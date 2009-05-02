;;;; Image Process Gimp script
;;;;
;;;; resides in ~/.gimp-2.2/scripts
;;;;
;;;; To run this from the command prompt:
;;;; gimp -c -d -i -b '(script-fu-image-process "file.jpg" tw th fw fh)' \
;;;;      '(gimp-quit 0)'
;;
(define (dds-thumbnailer filename outname width height)
  (let* ((img 0) (drw 0) (fileparts (strbreakup filename ".")))
    ;; car needed here because gimp functions return values as lists
    (set! img (car (gimp-file-load 1 filename filename)))
    ;; set image resolution to 72dpi
    (gimp-image-set-resolution img 72 72)
    ;; create 'full-size' image
    (gimp-image-scale img width height)
    ;; also flatten image to reduce byte storage even further
    (set! drw (car (gimp-image-flatten img)))

    (file-png-save 1 img (car (gimp-image-get-active-drawable img)) outname outname 1 0 0 0 0 0 0 )))
