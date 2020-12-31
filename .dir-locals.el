;;; $Id$
((c-mode . ((indent-tabs-mode .  nil)))
 (c++-mode . ((indent-tabs-mode .  nil)))
 (nil . ((flycheck-clang-language-standard . "c++11")
         (flycheck-clang-include-path . ("."
                                         "test"
                                         ))
	 (whitespace-style . (face tabs tab-mark trailing lines-tail empty))
         (c-file-style . "stroustrup")
         (eval . (add-to-list 'auto-mode-alist '("\\.h\\'" . c++-mode)))
         (eval . (if (boundp 'c-offsets-alist)
		     (progn 
		       (add-to-list 'c-offsets-alist '(innamespace . 0))
		       (add-to-list 'c-offsets-alist '(inextern-lang . 0)))))
	 (eval . (set (make-local-variable 'project-dir)
                      (file-name-directory
                       (let
                           ((d (dir-locals-find-file ".")))
                         (if (stringp d) d (car d)))) ))
	 (eval . (message "project-dir: '%s'" project-dir))
         (eval . (set (make-local-variable 'compile-command) (concat "cd " project-dir "; SDKROOT=macosx11.1 make.qmk && (cd test; SDKROOT=macosx11.1 make.qmk) && ./test/obj.Darwin/test")))
	 )))
;;; end of .dir-locals.el
