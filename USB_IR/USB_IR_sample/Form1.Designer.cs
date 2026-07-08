namespace USB_IR_sample
{
    partial class Form1
    {
        /// <summary>
        /// 必要なデザイナ変数です。
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// 使用中のリソースをすべてクリーンアップします。
        /// </summary>
        /// <param name="disposing">マネージ リソースが破棄される場合 true、破棄されない場合は false です。</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows フォーム デザイナで生成されたコード

        /// <summary>
        /// デザイナ サポートに必要なメソッドです。このメソッドの内容を
        /// コード エディタで変更しないでください。
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.btn_mode = new System.Windows.Forms.Button();
            this.pnl_remote = new System.Windows.Forms.Panel();
            this.lbl_status = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // btn_mode
            // 
            this.btn_mode.Location = new System.Drawing.Point(12, 12);
            this.btn_mode.Name = "btn_mode";
            this.btn_mode.Size = new System.Drawing.Size(248, 34);
            this.btn_mode.TabIndex = 0;
            this.btn_mode.Text = "実行モード";
            this.btn_mode.UseVisualStyleBackColor = true;
            this.btn_mode.Click += new System.EventHandler(this.btn_mode_Click);
            // 
            // pnl_remote
            // 
            this.pnl_remote.Location = new System.Drawing.Point(12, 58);
            this.pnl_remote.Name = "pnl_remote";
            this.pnl_remote.Size = new System.Drawing.Size(248, 440);
            this.pnl_remote.TabIndex = 1;
            // 
            // lbl_status
            // 
            this.lbl_status.Location = new System.Drawing.Point(12, 510);
            this.lbl_status.Name = "lbl_status";
            this.lbl_status.Size = new System.Drawing.Size(248, 36);
            this.lbl_status.TabIndex = 2;
            this.lbl_status.Text = "実行モード";
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 12F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(272, 558);
            this.Controls.Add(this.lbl_status);
            this.Controls.Add(this.pnl_remote);
            this.Controls.Add(this.btn_mode);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.Name = "Form1";
            this.Text = "USB赤外線リモコン";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button btn_mode;
        private System.Windows.Forms.Panel pnl_remote;
        private System.Windows.Forms.Label lbl_status;
    }
}
